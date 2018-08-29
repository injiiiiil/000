#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "generic/TemporalReplicationPadding.c"
#else

static void THNN_(TemporalReplicationPadding_updateOutput_frame)(
  real *input_p, real *output_p,
  long nslices,
  long iwidth,
  long owidth,
  int pad_l, int pad_r)
{
  int iStartX = fmax(0, -pad_l);
  int oStartX = fmax(0, pad_l);

  long k, ip_x;
#pragma omp parallel for private(k, ip_x)
  for (k = 0; k < nslices; k++)
  {
    long j;
    for (j = 0; j < owidth; j++) {
      if (j < pad_l) {
        ip_x = pad_l;
      } else if (j >= pad_l && j < iwidth + pad_l) {
        ip_x = j;
      } else {
        ip_x = iwidth + pad_l - 1;
      }
      ip_x = ip_x - oStartX + iStartX;

      real *dest_p = output_p + k*owidth + j;
      real *src_p = input_p + k*iwidth + ip_x;
      *dest_p = *src_p;
    }
  }
}

void THNN_(TemporalReplicationPadding_updateOutput)(THNNState *state,
                                                    THTensor *input,
                                                    THTensor *output,
                                                    int pad_l, int pad_r)
{
  int dimw = 1;
  int dimslices = 0;
  long nbatch = 1;
  long nslices;
  long iwidth;
  long owidth;
  real *input_data;
  real *output_data;

  THNN_ARGCHECK(!input->is_empty() && (input->dim() == 2 || input->dim() == 3), 2, input,
		"non-empty 2D or 3D (batch mode) tensor expected for input, but got: %s");

  if (input->dim() == 3)
  {
    nbatch = input->size(0);
    dimw++;
    dimslices++;
  }

  /* sizes */
  nslices = input->size(dimslices);
  iwidth = input->size(dimw);
  owidth  = iwidth + pad_l + pad_r;

  THArgCheck(owidth >= 1 , 2,
	     "input (W: %d)is too small."
	     " Calculated output W: %d",
	     iwidth, owidth);


  /* get contiguous input */
  input = THTensor_(newContiguous)(input);

  /* resize output */
  if (input->dim() == 2)
  {
    THTensor_(resize2d)(output, nslices, owidth);

    input_data = input->data<real>();
    output_data = output->data<real>();

    THNN_(TemporalReplicationPadding_updateOutput_frame)(input_data, output_data,
                                                    nslices,
                                                    iwidth,
                                                    owidth,
                                                    pad_l, pad_r);
  }
  else
  {
    long p;

    THTensor_(resize3d)(output, nbatch, nslices, owidth);

    input_data = input->data<real>();
    output_data = output->data<real>();

#pragma omp parallel for private(p)
    for (p = 0; p < nbatch; p++)
    {
      THNN_(TemporalReplicationPadding_updateOutput_frame)(
        input_data+p*nslices*iwidth,
        output_data+p*nslices*owidth,
        nslices,
        iwidth,
        owidth,
        pad_l, pad_r);
    }
  }

  /* cleanup */
  THTensor_(free)(input);
}

static void THNN_(TemporalReplicationPadding_updateGradInput_frame)(
  real *ginput_p, real *goutput_p,
  long nslices,
  long iwidth,
  long owidth,
  int pad_l, int pad_r)
{
  int iStartX = fmax(0, -pad_l);
  int oStartX = fmax(0, pad_l);

  long k, ip_x;
#pragma omp parallel for private(k, ip_x)
  for (k = 0; k < nslices; k++)
  {
    long j;
    for (j = 0; j < owidth; j++) {
      if (j < pad_l) {
        ip_x = pad_l;
      } else if (j >= pad_l && j < iwidth + pad_l) {
        ip_x = j;
      } else {
        ip_x = iwidth + pad_l - 1;
      }
      ip_x = ip_x - oStartX + iStartX;

      real *src_p = goutput_p + k*owidth + j;
      real *dest_p = ginput_p + k*iwidth + ip_x;
      *dest_p += *src_p;
    }
  }
}

void THNN_(TemporalReplicationPadding_updateGradInput)(THNNState *state,
                                                       THTensor *input,
                                                       THTensor *gradOutput,
                                                       THTensor *gradInput,
                                                       int pad_l, int pad_r)
{
  int dimw = 1;
  int dimslices = 0;
  long nbatch = 1;
  long nslices;
  long iwidth;
  long owidth;

  if (input->dim() == 3)
  {
    nbatch = input->size(0);
    dimw++;
    dimslices++;
  }

  /* sizes */
  nslices = input->size(dimslices);
  iwidth = input->size(dimw);
  owidth  = iwidth + pad_l + pad_r;

  THArgCheck(owidth == THTensor_(size)(gradOutput, dimw), 3,
	     "gradOutput width unexpected. Expected: %d, Got: %d",
	     owidth, THTensor_(size)(gradOutput, dimw));

  /* get contiguous gradOutput */
  gradOutput = THTensor_(newContiguous)(gradOutput);

  /* resize */
  THTensor_(resizeAs)(gradInput, input);
  THTensor_(zero)(gradInput);

  /* backprop */
  if (input->dim() == 2) {
    THNN_(TemporalReplicationPadding_updateGradInput_frame)(
      gradInput->data<real>(),
      gradOutput->data<real>(),
      nslices,
      iwidth,
      owidth,
      pad_l, pad_r);
  } else {
    long p;
#pragma omp parallel for private(p)
    for (p = 0; p < nbatch; p++) {
      THNN_(TemporalReplicationPadding_updateGradInput_frame)(
        gradInput->data<real>() + p * nslices * iwidth,
        gradOutput->data<real>() + p * nslices * owidth,
        nslices,
        iwidth,
        owidth,
        pad_l, pad_r);
    }
  }

  /* cleanup */
  THTensor_(free)(gradOutput);
}


#endif
