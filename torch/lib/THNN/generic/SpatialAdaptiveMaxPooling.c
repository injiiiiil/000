#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "generic/SpatialAdaptiveMaxPooling.c"
#else

static void THNN_(SpatialAdaptiveMaxPooling_updateOutput_frame)(
          real *input_p,
          real *output_p,
          THIndex_t *ind_p,
          int64_t sizeD,
          int64_t isizeW,
          int64_t isizeH,
          int64_t osizeW,
          int64_t osizeH,
          int64_t istrideW,
          int64_t istrideH,
          int64_t istrideD)
{
  int64_t d;
#pragma omp parallel for private(d)
  for (d = 0; d < sizeD; d++)
  {
    /* loop over output */
    int64_t oh, ow;
    for(oh = 0; oh < osizeH; oh++)
    {
      int istartH = (int)floor((float)oh / osizeH * isizeH);
      int iendH   = (int)ceil((float)(oh + 1) / osizeH * isizeH);
      int kH = iendH-istartH;

      for(ow = 0; ow < osizeW; ow++)
      {

        int istartW = (int)floor((float)ow / osizeW * isizeW);
        int iendW   = (int)ceil((float)(ow + 1) / osizeW * isizeW);
        int kW = iendW-istartW;

        /* local pointers */
        real *ip = input_p   + d*istrideD + istartH*istrideH + istartW*istrideW;
        real *op = output_p  + d*osizeW*osizeH + oh*osizeW + ow;
        THIndex_t *indp = ind_p   + d*osizeW*osizeH + oh*osizeW + ow;

        /* compute local max: */
        int64_t maxindex = -1;
        real maxval = -FLT_MAX;
        int64_t tcntr = 0;
        int iw,ih;
        for(ih = 0; ih < kH; ih++)
        {
          for(iw = 0; iw < kW; iw++)
          {
            real val = *(ip + ih*istrideH + iw*istrideW);
            if (val > maxval)
            {
              maxval = val;
              maxindex = (ih+istartH)*isizeW + (iw+istartW);
            }
          }
        }

        /* set output to local max */
        *op = maxval;

        /* store location of max */
        *indp = maxindex + TH_INDEX_BASE;
      }
    }
  }
}

void THNN_(SpatialAdaptiveMaxPooling_updateOutput)(
          THNNState *state,
          THTensor *input,
          THTensor *output,
          THIndexTensor *indices,
          int osizeW,
          int osizeH)
{
  int dimW = 2;
  int dimH = 1;
  int64_t sizeB = 1;
  int64_t sizeD;
  int64_t isizeH;
  int64_t isizeW;

  int64_t istrideD;
  int64_t istrideH;
  int64_t istrideW;
  int64_t istrideB;

  real *input_data;
  real *output_data;
  THIndex_t *indices_data;


  THNN_ARGCHECK(input->nDimension == 3 || input->nDimension == 4, 2, input,
		"3D or 4D (batch mode) tensor expected for input, but got: %s");

  if (input->nDimension == 4)
  {
    istrideB = input->stride[0];
    sizeB = input->size[0];
    dimW++;
    dimH++;
  }

  /* sizes */
  sizeD = input->size[dimH-1];
  isizeH = input->size[dimH];
  isizeW = input->size[dimW];
  /* strides */
  istrideD = input->stride[dimH-1];
  istrideH = input->stride[dimH];
  istrideW = input->stride[dimW];

  /* resize output */
  if (input->nDimension == 3)
  {
    THTensor_(resize3d)(output, sizeD, osizeH, osizeW);
    /* indices will contain i,j locations for each output point */
    THIndexTensor_(resize3d)(indices, sizeD, osizeH, osizeW);

    input_data = THTensor_(data)(input);
    output_data = THTensor_(data)(output);
    indices_data = THIndexTensor_(data)(indices);

    THNN_(SpatialAdaptiveMaxPooling_updateOutput_frame)(input_data, output_data,
                                                      indices_data,
                                                      sizeD,
                                                      isizeW, isizeH,
                                                      osizeW, osizeH,
                                                      istrideW,istrideH,
                                                      istrideD);
  }
  else
  {
    int64_t b;

    THTensor_(resize4d)(output, sizeB, sizeD, osizeH, osizeW);
    /* indices will contain i,j locations for each output point */
    THIndexTensor_(resize4d)(indices, sizeB, sizeD, osizeH, osizeW);

    input_data = THTensor_(data)(input);
    output_data = THTensor_(data)(output);
    indices_data = THIndexTensor_(data)(indices);

#pragma omp parallel for private(b)
    for (b = 0; b < sizeB; b++)
    {
      THNN_(SpatialAdaptiveMaxPooling_updateOutput_frame)(input_data+b*istrideB, output_data+b*sizeD*osizeW*osizeH,
                                                        indices_data+b*sizeD*osizeW*osizeH,
                                                        sizeD,
                                                        isizeW, isizeH,
                                                        osizeW, osizeH,
                                                        istrideW,istrideH,
                                                        istrideD);
    }
  }
}

static void THNN_(SpatialAdaptiveMaxPooling_updateGradInput_frame)(
          real *gradInput_p,
          real *gradOutput_p,
          THIndex_t *ind_p,
          int64_t sizeD,
          int64_t isizeW,
          int64_t isizeH,
          int64_t osizeW,
          int64_t osizeH)
{
  int64_t d;
#pragma omp parallel for private(d)
  for (d = 0; d < sizeD; d++)
  {
    real *gradInput_p_d = gradInput_p + d*isizeW*isizeH;
    real *gradOutput_p_d = gradOutput_p + d*osizeW*osizeH;
    THIndex_t *ind_p_d = ind_p + d*osizeW*osizeH;

    /* calculate max points */
    int64_t oh, ow;
    for(oh = 0; oh < osizeH; oh++)
    {
      int istartH = (int)floor((float) oh / osizeH * isizeH);
      for(ow = 0; ow < osizeW; ow++)
      {
        int istartW = (int)floor((float) ow / osizeW * isizeW);
        /* retrieve position of max */
        int64_t maxp = ind_p_d[oh*osizeW + ow] - TH_INDEX_BASE;

        /* update gradient */
        gradInput_p_d[maxp] += gradOutput_p_d[oh*osizeW + ow];
      }
    }
  }
}

void THNN_(SpatialAdaptiveMaxPooling_updateGradInput)(
          THNNState *state,
          THTensor *input,
          THTensor *gradOutput,
          THTensor *gradInput,
          THIndexTensor *indices)
{
  int dimW = 2;
  int dimH = 1;
  int64_t sizeB = 1;
  int sizeD;
  int isizeH;
  int isizeW;
  int osizeH;
  int osizeW;
  real *gradInput_data;
  real *gradOutput_data;
  THIndex_t *indices_data;

  /* get contiguous gradOutput */
  gradOutput = THTensor_(newContiguous)(gradOutput);

  /* resize */
  THTensor_(resizeAs)(gradInput, input);
  THTensor_(zero)(gradInput);

  if (input->nDimension == 4) {
    sizeB = input->size[0];
    dimW++;
    dimH++;
  }

  /* sizes */
  sizeD = input->size[dimH-1];
  isizeH = input->size[dimH];
  isizeW = input->size[dimW];
  osizeH = gradOutput->size[dimH];
  osizeW = gradOutput->size[dimW];

  /* get raw pointers */
  gradInput_data = THTensor_(data)(gradInput);
  gradOutput_data = THTensor_(data)(gradOutput);
  indices_data = THIndexTensor_(data)(indices);

  /* backprop */
  if (input->nDimension == 3)
  {
    THNN_(SpatialAdaptiveMaxPooling_updateGradInput_frame)(gradInput_data, gradOutput_data,
                                                           indices_data,
                                                           sizeD,
                                                           isizeW, isizeH,
                                                           osizeW, osizeH);
  }
  else
  {
    int64_t b;
#pragma omp parallel for private(b)
    for (b = 0; b < sizeB; b++)
    {
      THNN_(SpatialAdaptiveMaxPooling_updateGradInput_frame)(gradInput_data+b*sizeD*isizeW*isizeH, gradOutput_data+b*sizeD*osizeW*osizeH,
                                                             indices_data+b*sizeD*osizeW*osizeH,
                                                             sizeD,
                                                             isizeW, isizeH,
                                                             osizeW, osizeH);
    }
  }

  /* cleanup */
  THTensor_(free)(gradOutput);
}

#endif
