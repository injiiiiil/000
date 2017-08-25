/* Automatically generated nanopb header */
/* Generated by nanopb-0.3.9-dev */

#ifndef PB_TOFFEE_TOFFEE_PB_H_INCLUDED
#define PB_TOFFEE_TOFFEE_PB_H_INCLUDED
#include <pb.h>

/* @@protoc_insertion_point(includes) */
#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
typedef enum _toffee_Version {
    toffee_Version_CURRENT_VERSION = 1
} toffee_Version;
#define _toffee_Version_MIN toffee_Version_CURRENT_VERSION
#define _toffee_Version_MAX toffee_Version_CURRENT_VERSION
#define _toffee_Version_ARRAYSIZE ((toffee_Version)(toffee_Version_CURRENT_VERSION+1))

typedef enum _toffee_TensorProto_DataType {
    toffee_TensorProto_DataType_UNDEFINED = 0,
    toffee_TensorProto_DataType_FLOAT = 1,
    toffee_TensorProto_DataType_DOUBLE = 2,
    toffee_TensorProto_DataType_UINT8 = 3,
    toffee_TensorProto_DataType_INT8 = 4,
    toffee_TensorProto_DataType_UINT16 = 5,
    toffee_TensorProto_DataType_INT16 = 6,
    toffee_TensorProto_DataType_INT32 = 7,
    toffee_TensorProto_DataType_INT64 = 8,
    toffee_TensorProto_DataType_STRING = 9,
    toffee_TensorProto_DataType_BOOL = 10,
    toffee_TensorProto_DataType_FLOAT16 = 11
} toffee_TensorProto_DataType;
#define _toffee_TensorProto_DataType_MIN toffee_TensorProto_DataType_UNDEFINED
#define _toffee_TensorProto_DataType_MAX toffee_TensorProto_DataType_FLOAT16
#define _toffee_TensorProto_DataType_ARRAYSIZE ((toffee_TensorProto_DataType)(toffee_TensorProto_DataType_FLOAT16+1))

/* Struct definitions */
typedef struct _toffee_NodeProto {
    pb_callback_t input;
    pb_callback_t output;
    pb_callback_t name;
    pb_callback_t op_type;
    pb_callback_t attribute;
/* @@protoc_insertion_point(struct:toffee_NodeProto) */
} toffee_NodeProto;

typedef struct _toffee_AttributeProto {
    pb_callback_t name;
    bool has_f;
    float f;
    bool has_i;
    int64_t i;
    pb_callback_t s;
    pb_callback_t floats;
    pb_callback_t ints;
    pb_callback_t strings;
    pb_callback_t tensors;
    pb_callback_t graphs;
/* @@protoc_insertion_point(struct:toffee_AttributeProto) */
} toffee_AttributeProto;

typedef struct _toffee_GraphProto {
    bool has_version;
    int64_t version;
    pb_callback_t node;
    pb_callback_t name;
    pb_callback_t input;
    pb_callback_t output;
    pb_callback_t initializer;
/* @@protoc_insertion_point(struct:toffee_GraphProto) */
} toffee_GraphProto;

typedef struct _toffee_TensorProto_Segment {
    bool has_begin;
    int64_t begin;
    bool has_end;
    int64_t end;
/* @@protoc_insertion_point(struct:toffee_TensorProto_Segment) */
} toffee_TensorProto_Segment;

typedef struct _toffee_TensorProto {
    pb_callback_t dims;
    bool has_data_type;
    toffee_TensorProto_DataType data_type;
    pb_callback_t float_data;
    pb_callback_t double_data;
    pb_callback_t int32_data;
    pb_callback_t string_data;
    pb_callback_t int64_data;
    pb_callback_t name;
    bool has_segment;
    toffee_TensorProto_Segment segment;
    pb_callback_t raw_data;
/* @@protoc_insertion_point(struct:toffee_TensorProto) */
} toffee_TensorProto;

typedef struct _toffee_SparseTensorProto {
    pb_callback_t dims;
    bool has_indices;
    toffee_TensorProto indices;
    bool has_values;
    toffee_TensorProto values;
/* @@protoc_insertion_point(struct:toffee_SparseTensorProto) */
} toffee_SparseTensorProto;

/* Default values for struct fields */

/* Initializer values for message structs */
#define toffee_AttributeProto_init_default       {{{NULL}, NULL}, false, 0, false, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define toffee_NodeProto_init_default            {{{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define toffee_GraphProto_init_default           {false, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define toffee_TensorProto_init_default          {{{NULL}, NULL}, false, (toffee_TensorProto_DataType)0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, false, toffee_TensorProto_Segment_init_default, {{NULL}, NULL}}
#define toffee_TensorProto_Segment_init_default  {false, 0, false, 0}
#define toffee_SparseTensorProto_init_default    {{{NULL}, NULL}, false, toffee_TensorProto_init_default, false, toffee_TensorProto_init_default}
#define toffee_AttributeProto_init_zero          {{{NULL}, NULL}, false, 0, false, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define toffee_NodeProto_init_zero               {{{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define toffee_GraphProto_init_zero              {false, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define toffee_TensorProto_init_zero             {{{NULL}, NULL}, false, (toffee_TensorProto_DataType)0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, false, toffee_TensorProto_Segment_init_zero, {{NULL}, NULL}}
#define toffee_TensorProto_Segment_init_zero     {false, 0, false, 0}
#define toffee_SparseTensorProto_init_zero       {{{NULL}, NULL}, false, toffee_TensorProto_init_zero, false, toffee_TensorProto_init_zero}

/* Field tags (for use in manual encoding/decoding) */
#define toffee_NodeProto_input_tag               1
#define toffee_NodeProto_output_tag              2
#define toffee_NodeProto_name_tag                3
#define toffee_NodeProto_op_type_tag             4
#define toffee_NodeProto_attribute_tag           5
#define toffee_AttributeProto_name_tag           1
#define toffee_AttributeProto_f_tag              2
#define toffee_AttributeProto_i_tag              3
#define toffee_AttributeProto_s_tag              4
#define toffee_AttributeProto_floats_tag         5
#define toffee_AttributeProto_ints_tag           6
#define toffee_AttributeProto_strings_tag        7
#define toffee_AttributeProto_tensors_tag        8
#define toffee_AttributeProto_graphs_tag         9
#define toffee_GraphProto_version_tag            1
#define toffee_GraphProto_node_tag               2
#define toffee_GraphProto_name_tag               3
#define toffee_GraphProto_input_tag              4
#define toffee_GraphProto_output_tag             5
#define toffee_GraphProto_initializer_tag        6
#define toffee_TensorProto_Segment_begin_tag     1
#define toffee_TensorProto_Segment_end_tag       2
#define toffee_TensorProto_dims_tag              1
#define toffee_TensorProto_data_type_tag         2
#define toffee_TensorProto_float_data_tag        3
#define toffee_TensorProto_double_data_tag       4
#define toffee_TensorProto_int32_data_tag        5
#define toffee_TensorProto_string_data_tag       6
#define toffee_TensorProto_int64_data_tag        7
#define toffee_TensorProto_name_tag              8
#define toffee_TensorProto_segment_tag           9
#define toffee_TensorProto_raw_data_tag          10
#define toffee_SparseTensorProto_dims_tag        1
#define toffee_SparseTensorProto_indices_tag     2
#define toffee_SparseTensorProto_values_tag      3

/* Struct field encoding specification for nanopb */
extern const pb_field_t toffee_AttributeProto_fields[10];
extern const pb_field_t toffee_NodeProto_fields[6];
extern const pb_field_t toffee_GraphProto_fields[7];
extern const pb_field_t toffee_TensorProto_fields[11];
extern const pb_field_t toffee_TensorProto_Segment_fields[3];
extern const pb_field_t toffee_SparseTensorProto_fields[4];

/* Maximum encoded size of messages (where known) */
/* toffee_AttributeProto_size depends on runtime parameters */
/* toffee_NodeProto_size depends on runtime parameters */
/* toffee_GraphProto_size depends on runtime parameters */
/* toffee_TensorProto_size depends on runtime parameters */
#define toffee_TensorProto_Segment_size          22
/* toffee_SparseTensorProto_size depends on runtime parameters */

/* Message IDs (where set with "msgid" option) */
#ifdef PB_MSGID

#define TOFFEE_MESSAGES \


#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
/* @@protoc_insertion_point(eof) */

#endif
