from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from caffe2.python import core, schema
from caffe2.python.layers.layers import (
    IdList,
    IdScoreList,
    LayerParameter,
    ModelLayer,
)
import functools
import math
import numpy as np
import operator


class SparseLookup(ModelLayer):
    _supported_reducers = ['PositionWeighted', 'LogMeanExp', 'LogSumExp', 'Max',
                           'Mean', 'Sum']

    def __init__(self, model, input_record, inner_shape, reducer,
                 weight_init=None, weight_optim=None,
                 name='sparse_lookup', **kwargs):
        super(SparseLookup, self).__init__(model, name, input_record, **kwargs)

        if isinstance(inner_shape, int):
            inner_shape = [inner_shape]
        assert isinstance(inner_shape, list) or isinstance(inner_shape, tuple),\
            "Unexpected type for inner_shape, expected list or tuple, got {0}".\
            format(type(inner_shape))

        # TODO Add some asserts about input type
        assert reducer in self._supported_reducers, "Unsupported reducer: {}".\
            format(reducer)
        self.reducer = reducer

        assert input_record.items.metadata is not None,\
            "Features without metadata are not supported"
        input_dim = input_record.items.metadata.categorical_limit
        assert input_dim is not None, "Unbounded features are not supported"

        self.output_schema = schema.Scalar(
            (np.float32, inner_shape),
            model.net.NextScopedBlob(name + '_output'),
        )

        if self.request_only:
            schema.attach_metadata_to_scalars(
                self.output_schema,
                schema.Metadata(
                    categorical_limit=None,
                    expected_value=None,
                    feature_specs=schema.FeatureSpec(
                        feature_is_request_only=True
                    )
                )
            )
        scale = math.sqrt(1.0 / input_dim)
        self.shape = [input_dim] + inner_shape
        self.weight_init = weight_init if weight_init else (
            'UniformFill', {'min': -scale, 'max': scale})

        self.w = model.net.NextScopedBlob(name + "_w")
        self.params.append(
            LayerParameter(
                parameter=self.w,
                initializer=core.CreateOperator(self.weight_init[0],
                                                [],
                                                self.w,
                                                shape=self.shape,
                                                **self.weight_init[1]
                                                ),
                optimizer=weight_optim
            ))

        if reducer == 'PositionWeighted':
            self.pos_w = model.net.NextScopedBlob(name + "_pos_w")
            self.params.append(
                LayerParameter(
                    parameter=self.pos_w,
                    initializer=core.CreateOperator('ConstantFill',
                                                    [],
                                                    self.pos_w,
                                                    shape=[input_dim, ],
                                                    value=1.0
                                                    ),
                    optimizer=weight_optim
                ))

    def get_memory_usage(self):
        return functools.reduce(operator.mul, self.shape) * 4

    def get_fp16_compatible_parameters(self):
        if (self.reducer == 'Sum' and
                schema.equal_schemas(self.input_record, IdList)):
            return [self.w]
        return []

    def add_ops(self, net):
        if schema.equal_schemas(self.input_record, IdList):
            if self.reducer == 'Sum':
                net.SparseLengthsSum(
                    [
                        self.w,
                        self.input_record.items(),
                        self.input_record.lengths()
                    ],
                    self.output_schema.field_blobs()
                )
            elif self.reducer == 'PositionWeighted':
                inc_seq = net.LengthsRangeFill(
                    [self.input_record.lengths()],
                    self.input_record.lengths() + '_seq'
                )
                gather_pos_w = net.Gather(
                    [self.pos_w, inc_seq], self.pos_w + '_gather')

                net.SparseLengthsWeightedSum(
                    [
                        self.w,
                        gather_pos_w,
                        self.input_record.items(),
                        self.input_record.lengths()
                    ],
                    self.output_schema.field_blobs(),
                    grad_on_weights=1
                )
            else:
                table_rows = net.Gather([self.w, self.input_record.keys()])
                segment_ids = net.LengthsToSegmentIds(
                    self.input_record.lengths())
                net.__getattr__('SortedSegmentRange' + self.reducer)(
                    [table_rows, segment_ids],
                    self.output_schema.field_blobs()
                )
        elif schema.equal_schemas(self.input_record, IdScoreList):
            if self.reducer == 'Sum':
                net.SparseLengthsWeightedSum(
                    [
                        self.w,
                        self.input_record.values(),
                        self.input_record.keys(),
                        self.input_record.lengths()
                    ],
                    self.output_schema.field_blobs()
                )
            else:
                raise "Only Sum is supported for IdScoreList input." +\
                    "Trying to create with {}".format(self.reducer)
        else:
            raise "Unsupported input type {0}".format(self.input_record)
