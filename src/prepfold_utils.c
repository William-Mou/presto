#include "prepfold.h"
#include "prepfold_cmd.h"

int compare_doubles(const void *a, const void *b);

double switch_pfdot(double pf, double pfdot)
{
  double retval;

  if (pf == 0.0)
    return 0.0;
  else {
    retval = -pfdot / (pf * pf);
    if (retval==-0)
      return 0.0;
    else
      return retval;
  }
}

double switch_pfdotdot(double pf, double pfdot, double pfdotdot)
{
  double retval;

  if (pf == 0.0 || pfdotdot == 0.0)
    return 0.0;
  else {
    retval = 2.0 * pfdot * pfdot / (pf * pf * pf) - pfdotdot / (pf * pf);
    if (retval==-0)
      return 0.0;
    else
      return retval;
  }
}

double fdot2phasedelay(double fdot, double time)
{
  double retval;

  retval = fdot * time * time / 2.0;
  if (retval==-0)
    return 0.0;
  else
    return retval;
}

double fdotdot2phasedelay(double fdotdot, double time)
{
  double retval;

  retval = fdotdot * time * time * time / 6.0;
  if (retval==-0)
    return 0.0;
  else
    return retval;
}

double phasedelay2fdot(double phasedelay, double time)
{
  if (time == 0.0)
    return 0.0;
  else 
    return 2.0 * phasedelay / (time * time);
}

double phasedelay2fdotdot(double phasedelay, double time)
{
  if (time == 0.0)
    return 0.0;
  else 
    return 6.0 * phasedelay / (time * time * time);
}


int read_floats(FILE *file, float *data, int numpts, int numchan)
/* This routine reads a numpts records of numchan each from */
/* the input file *file which contains normal floating      */
/* point data.                                              */
/* It returns the number of points read.                    */
{
  return chkfread(data, sizeof(float),
		  (unsigned long) (numpts * numchan), file) / numchan;
}


int read_shorts(FILE *file, float *data, int numpts, int numchan)
/* This routine reads a numpts records of numchan each from */
/* the input file *file which contains short integer data.  */
/* The equivalent floats are placed in *data.               */
/* It returns the number of points read.                    */
{
  short *sdata;
  int ii, numread;

  sdata = (short *) malloc((size_t) (sizeof(short) * (numpts * numchan)));
  if (!sdata) {
    perror("\nError allocating short array in read_shorts()");
    printf("\n");
    exit(-1);
  }
  numread = chkfread(sdata, sizeof(short),
		     (unsigned long) (numpts * numchan), file) / numchan;
  for (ii=0; ii<numread; ii++)
    data[ii] = (float) sdata[ii];
  free(sdata);
  return numread;
}


double *read_events(FILE *infile, int bin, int days, int *numevents,
		    double MJD0, double Ttot, double startfrac, double endfrac, 
		    double offset)
/* This routine reads a set of events from the open file 'infile'.     */
/* It returns a double precision vector of events in seconds from the  */
/* first event.  If 'bin' is true the routine treats the data as       */
/* binary double precision (otherwise text).  If 'days' is 1 then the  */
/* data is assumed to be in days since the 'inf' EPOCH (0 is sec from  */
/* EPOCH in 'inf').  If 'days' is 2, the data are assumed to be MJDs.  */
/* The number of events read is placed in 'numevents', and the raw     */
/* event is placed in 'firstevent'.  MJD0 is the time to use for the   */
/* reference time.  Ttot is the duration of the observation. 'start'   */
/* and 'end' are define the fraction of the observation that we are    */
/* interested in.  'offset' is a time offset to apply to the events.   */
{
  int N=0, nn=0, goodN=0;
  double *ts, *goodts, dtmp, lotime, hitime;
  char line[80];

  if (bin){
    N = chkfilelen(infile, sizeof(double));
  } else {
    /* Read the input file once to count events */
    while (!feof(infile)){
      fgets(line, 80, infile);
      if (line[0]!='#')
	if (sscanf(line, "%lf", &dtmp)==1) N++;
    }
  }

  /* Allocate the event arrays */

  ts = (double *)malloc(N * sizeof(double));

  /* Rewind and read the events for real */

  rewind(infile);
  if (bin){
    fread(ts, sizeof(double), N, infile);
  } else {
    while (!feof(infile)){
      fgets(line, 80, infile);
      if (line[0]!='#')
	if (sscanf(line, "%lf", &ts[nn])==1) nn++;
    }
  }

  /* Convert all the events to MJD */

  if (days==0){ /* Events are in seconds since MJD0 */
    for (nn=0; nn<N; nn++)
      ts[nn] = MJD0+(ts[nn]+offset)/SECPERDAY;
  } else if (days==1){ /* Events are in days since MJD0 */
    for (nn=0; nn<N; nn++)
      ts[nn] = MJD0+(ts[nn]+offset);
  } else if (days==2 && 
	     offset != 0.0){ /* Events are in MJD with an offset */
    for (nn=0; nn<N; nn++)
      ts[nn] += offset;
  }

  /* Count how many events are within our range and only keep them */

  lotime = MJD0 + startfrac*Ttot/SECPERDAY;
  hitime = MJD0 + endfrac*Ttot/SECPERDAY;
  for (nn=0; nn<N; nn++)
    if (ts[nn] >= lotime && ts[nn] < hitime) goodN++;
  if (goodN != N){
    goodts = (double *)malloc(goodN * sizeof(double));
    goodN = 0;
    for (nn=0; nn<N; nn++){
      if (ts[nn] >= lotime && ts[nn] < hitime){
	goodts[goodN] = ts[nn];
	goodN++;
      }
    }
    free(ts);
    ts = goodts;
    N = goodN;
  } else {
    goodts = ts;
  }
  *numevents = N;

  /* Convert the events to seconds from MJD0 */

  for (nn=0; nn<N; nn++)
    goodts[nn] = (goodts[nn]-MJD0)*SECPERDAY;

  /* Sort the events and return them */

  qsort(goodts, N, sizeof(double), compare_doubles); 
  return goodts;
}



int bary2topo(double *topotimes, double *barytimes, int numtimes, 
	      double fb, double fbd, double fbdd, 
	      double *ft, double *ftd, double *ftdd)
/* Convert a set of barycentric pulsar spin parameters (fb, fbd, fbdd) */
/* into topocentric spin parameters (ft, ftd, ftdd) by performing      */
/* a linear least-squares fit (using LAPACK routine DGELS).  The       */
/* routine equates the pulse phase using topcentric parameters and     */
/* times to the pulse phase using barycentric parameters and times.    */
{
  double *work, *aa, *bb, dtmp;
  int ii, mm=3, nn, nrhs=1, lwork, info, index;
  char trans='T';

  if (numtimes < 4){
    printf("\n'numtimes' < 4 in bary2topo():  Cannot solve.\n\n");
    exit(0);
  }
  nn = numtimes; 
  lwork = mm + nn * 9;
  aa = gen_dvect(mm * nn);
  bb = gen_dvect(nn);
  work = gen_dvect(lwork);
  for (ii = 0; ii < nn; ii++){
    index = ii * 3;
    dtmp = (topotimes[ii] - topotimes[0]) * SECPERDAY;
    aa[index] = dtmp;
    aa[index+1] = 0.5 * dtmp * dtmp;
    aa[index+2] = dtmp * dtmp * dtmp / 6.0;
    dtmp = (barytimes[ii] - barytimes[0]) * SECPERDAY;
    bb[ii] = dtmp * (fb + dtmp * (0.5 * fbd + fbdd * dtmp / 6.0));
  }
  dgels_(&trans, &mm, &nn, &nrhs, aa, &mm, bb, &nn, work, &lwork, &info);
  *ft = bb[0];
  *ftd = bb[1];
  *ftdd = bb[2];
  free(aa);
  free(bb);
  free(work);
  return info;
}


void init_prepfoldinfo(prepfoldinfo *in)
/* Set all values to 0 or NULL */
{
  in->rawfolds = NULL;
  in->dms = NULL;
  in->periods = NULL;
  in->pdots = NULL;
  in->stats = NULL;
  in->numdms = 0;
  in->numperiods = 0;
  in->numpdots = 0;
  in->nsub = 0;
  in->npart = 0;
  in->proflen = 0;
  in->numchan = 1;
  in->ndmfact = 2;
  in->npfact = 1;
  in->pstep = 1;
  in->pdstep = 1;
  in->dmstep = 1;
  in->filenm = NULL;
  in->candnm = NULL;
  in->telescope = NULL;
  in->pgdev = NULL;
  in->dt = 0.0;
  in->startT = 0.0;
  in->endT = 0.0;
  in->tepoch = 0.0;
  in->bepoch = 0.0;
  in->avgvoverc = 0.0;
  in->lofreq = 0.0;
  in->chan_wid = 0.0;
  in->bestdm = 0.0;
  in->topo.pow = 0.0;
  in->topo.p1 = 0.0;
  in->topo.p2 = 0.0;
  in->topo.p3 = 0.0;
  in->bary.pow = 0.0;
  in->bary.p1 = 0.0;
  in->bary.p2 = 0.0;
  in->bary.p3 = 0.0;
  in->fold.pow = 0.0;
  in->fold.p1 = 0.0;
  in->fold.p2 = 0.0;
  in->fold.p3 = 0.0;
  in->orb.p = 0.0;
  in->orb.e = 0.0;
  in->orb.x = 0.0;
  in->orb.w = 0.0;
  in->orb.t = 0.0;
  in->orb.pd = 0.0;
  in->orb.wd = 0.0;
}

void print_prepfoldinfo(prepfoldinfo *in)
/* Print a prepfoldinfo data structure to STDOUT */
{
  printf("\n\n");
  printf("numdms      =  %d\n", in->numdms);
  printf("numperiods  =  %d\n", in->numperiods);
  printf("numpdots    =  %d\n", in->numpdots);
  printf("nsub        =  %d\n", in->nsub);
  printf("npart       =  %d\n", in->npart);
  printf("proflen     =  %d\n", in->proflen);
  printf("numchan     =  %d\n", in->numchan);
  printf("pstep       =  %d\n", in->pstep);
  printf("pdstep      =  %d\n", in->pdstep);
  printf("dmstep      =  %d\n", in->dmstep);
  printf("ndmfact     =  %d\n", in->ndmfact);
  printf("npfact      =  %d\n", in->npfact);
  printf("filenm      =  '%s'\n", in->filenm);
  printf("candnm      =  '%s'\n", in->candnm);
  printf("telescope   =  '%s'\n", in->telescope);
  printf("pgdev       =  '%s'\n", in->pgdev);
  printf("dt          =  %.14g\n", in->dt);
  printf("startT      =  %.14g\n", in->startT);
  printf("endT        =  %.14g\n", in->endT);
  printf("tepoch      =  %.14g\n", in->tepoch);
  printf("bepoch      =  %.14g\n", in->bepoch);
  printf("avgvoverc   =  %.14g\n", in->avgvoverc);
  printf("lofreq      =  %.14g\n", in->lofreq);
  printf("chan_wid    =  %.14g\n", in->chan_wid);
  printf("bestdm      =  %.14g\n", in->bestdm);
  printf("topo.pow    =  %.14g\n", in->topo.pow);
  printf("topo.p1     =  %.14g\n", in->topo.p1);
  printf("topo.p2     =  %.14g\n", in->topo.p2);
  printf("topo.p3     =  %.14g\n", in->topo.p3);
  printf("bary.pow    =  %.14g\n", in->bary.pow);
  printf("bary.p1     =  %.14g\n", in->bary.p1);
  printf("bary.p2     =  %.14g\n", in->bary.p2);
  printf("bary.p3     =  %.14g\n", in->bary.p3);
  printf("fold.pow    =  %.14g\n", in->fold.pow);
  printf("fold.p1     =  %.14g\n", in->fold.p1);
  printf("fold.p2     =  %.14g\n", in->fold.p2);
  printf("fold.p3     =  %.14g\n", in->fold.p3);
  printf("orb.p       =  %.14g\n", in->orb.p);
  printf("orb.e       =  %.14g\n", in->orb.e);
  printf("orb.x       =  %.14g\n", in->orb.x);
  printf("orb.w       =  %.14g\n", in->orb.w);
  printf("orb.t       =  %.14g\n", in->orb.t);
  printf("orb.pd      =  %.14g\n", in->orb.pd);
  printf("orb.wd      =  %.14g\n", in->orb.wd);
  printf("\n\n");
}

void write_prepfoldinfo(prepfoldinfo *in, char *filename)
/* Write a prepfoldinfo data structure to a binary file */
{
  FILE *outfile;
  int itmp;

  outfile = chkfopen(filename, "wb");
  chkfwrite(&in->numdms, sizeof(int), 1, outfile);
  chkfwrite(&in->numperiods, sizeof(int), 1, outfile);
  chkfwrite(&in->numpdots, sizeof(int), 1, outfile);
  chkfwrite(&in->nsub, sizeof(int), 1, outfile);
  chkfwrite(&in->npart, sizeof(int), 1, outfile);
  chkfwrite(&in->proflen, sizeof(int), 1, outfile);
  chkfwrite(&in->numchan, sizeof(int), 1, outfile);
  chkfwrite(&in->pstep, sizeof(int), 1, outfile);
  chkfwrite(&in->pdstep, sizeof(int), 1, outfile);
  chkfwrite(&in->dmstep, sizeof(int), 1, outfile);
  chkfwrite(&in->ndmfact, sizeof(int), 1, outfile);
  chkfwrite(&in->npfact, sizeof(int), 1, outfile);
  itmp = strlen(in->filenm);
  chkfwrite(&itmp, sizeof(int), 1, outfile);
  chkfwrite(in->filenm, sizeof(char), itmp, outfile);
  itmp = strlen(in->candnm);
  chkfwrite(&itmp, sizeof(int), 1, outfile);
  chkfwrite(in->candnm, sizeof(char), itmp, outfile);
  itmp = strlen(in->telescope);
  chkfwrite(&itmp, sizeof(int), 1, outfile);
  chkfwrite(in->telescope, sizeof(char), itmp, outfile);
  itmp = strlen(in->pgdev);
  chkfwrite(&itmp, sizeof(int), 1, outfile);
  chkfwrite(in->pgdev, sizeof(char), itmp, outfile);
  chkfwrite(&in->dt, sizeof(double), 1, outfile);
  chkfwrite(&in->startT, sizeof(double), 1, outfile);
  chkfwrite(&in->endT, sizeof(double), 1, outfile);
  chkfwrite(&in->tepoch, sizeof(double), 1, outfile);
  chkfwrite(&in->bepoch, sizeof(double), 1, outfile);
  chkfwrite(&in->avgvoverc, sizeof(double), 1, outfile);
  chkfwrite(&in->lofreq, sizeof(double), 1, outfile);
  chkfwrite(&in->chan_wid, sizeof(double), 1, outfile);
  chkfwrite(&in->bestdm, sizeof(double), 1, outfile);
  chkfwrite(&(in->topo.pow), sizeof(double), 1, outfile);
  chkfwrite(&(in->topo.p1), sizeof(double), 1, outfile);
  chkfwrite(&(in->topo.p2), sizeof(double), 1, outfile);
  chkfwrite(&(in->topo.p3), sizeof(double), 1, outfile);
  chkfwrite(&(in->bary.pow), sizeof(double), 1, outfile);
  chkfwrite(&(in->bary.p1), sizeof(double), 1, outfile);
  chkfwrite(&(in->bary.p2), sizeof(double), 1, outfile);
  chkfwrite(&(in->bary.p3), sizeof(double), 1, outfile);
  chkfwrite(&(in->fold.pow), sizeof(double), 1, outfile);
  chkfwrite(&(in->fold.p1), sizeof(double), 1, outfile);
  chkfwrite(&(in->fold.p2), sizeof(double), 1, outfile);
  chkfwrite(&(in->fold.p3), sizeof(double), 1, outfile);
  chkfwrite(&(in->orb.p), sizeof(double), 1, outfile);
  chkfwrite(&(in->orb.e), sizeof(double), 1, outfile);
  chkfwrite(&(in->orb.x), sizeof(double), 1, outfile);
  chkfwrite(&(in->orb.w), sizeof(double), 1, outfile);
  chkfwrite(&(in->orb.t), sizeof(double), 1, outfile);
  chkfwrite(&(in->orb.pd), sizeof(double), 1, outfile);
  chkfwrite(&(in->orb.wd), sizeof(double), 1, outfile);
  chkfwrite(in->dms, sizeof(double), in->numdms, outfile);
  chkfwrite(in->periods, sizeof(double), in->numperiods, outfile);
  chkfwrite(in->pdots, sizeof(double), in->numpdots, outfile);
  chkfwrite(in->rawfolds, sizeof(double), in->nsub * 
	    in->npart * in->proflen, outfile);
  chkfwrite(in->stats, sizeof(foldstats), 
	    in->nsub * in->npart, outfile);
  fclose(outfile);
}

void read_prepfoldinfo(prepfoldinfo *in, char *filename)
/* Read a prepfoldinfo data structure from a binary file */
{
  FILE *infile;
  int itmp, byteswap=0;
  float ftmp;

  infile = chkfopen(filename, "rb");
  in->numdms = read_int(infile, byteswap);
  in->numperiods = read_int(infile, byteswap);
  in->numpdots = read_int(infile, byteswap);
  in->nsub = read_int(infile, byteswap);
  in->npart = read_int(infile, byteswap);
  /* The following is not exactly the most robust, but it should work... */
  if (in->npart < 1 || in->npart > 10000){
    byteswap = 1;
    in->numdms = swap_int(in->numdms);
    in->numperiods = swap_int(in->numperiods);
    in->numpdots = swap_int(in->numpdots);
    in->nsub = swap_int(in->nsub);
    in->npart = swap_int(in->npart);
  }
  in->proflen = read_int(infile, byteswap);
  in->numchan = read_int(infile, byteswap);
  in->pstep = read_int(infile, byteswap);
  in->pdstep = read_int(infile, byteswap);
  in->dmstep = read_int(infile, byteswap);
  in->ndmfact = read_int(infile, byteswap);
  in->npfact = read_int(infile, byteswap);
  itmp = read_int(infile, byteswap);
  in->filenm = calloc(itmp+1, sizeof(char));
  chkfread(in->filenm, sizeof(char), itmp, infile);
  itmp = read_int(infile, byteswap);
  in->candnm = calloc(itmp+1, sizeof(char));
  chkfread(in->candnm, sizeof(char), itmp, infile);
  itmp = read_int(infile, byteswap);
  in->telescope = calloc(itmp+1, sizeof(char));
  chkfread(in->telescope, sizeof(char), itmp, infile);
  itmp = read_int(infile, byteswap);
  in->pgdev = calloc(itmp+1, sizeof(char));
  chkfread(in->pgdev, sizeof(char), itmp, infile);
  in->dt = read_double(infile, byteswap);
  in->startT = read_double(infile, byteswap);
  in->endT = read_double(infile, byteswap);
  in->tepoch = read_double(infile, byteswap);
  in->bepoch = read_double(infile, byteswap);
  in->avgvoverc = read_double(infile, byteswap);
  in->lofreq = read_double(infile, byteswap);
  in->chan_wid = read_double(infile, byteswap);
  in->bestdm = read_double(infile, byteswap);
  /* The .pow elements were written as doubles (Why??) */
  in->topo.pow = read_float(infile, byteswap);
  ftmp = read_float(infile, byteswap);
  in->topo.p1 = read_double(infile, byteswap);
  in->topo.p2 = read_double(infile, byteswap);
  in->topo.p3 = read_double(infile, byteswap);
  /* The .pow elements were written as doubles (Why??) */
  in->bary.pow = read_float(infile, byteswap);
  ftmp = read_float(infile, byteswap);
  in->bary.p1 = read_double(infile, byteswap);
  in->bary.p2 = read_double(infile, byteswap);
  in->bary.p3 = read_double(infile, byteswap);
  /* The .pow elements were written as doubles (Why??) */
  in->fold.pow = read_float(infile, byteswap);
  ftmp = read_float(infile, byteswap);
  in->fold.p1 = read_double(infile, byteswap);
  in->fold.p2 = read_double(infile, byteswap);
  in->fold.p3 = read_double(infile, byteswap);
  in->orb.p = read_double(infile, byteswap);
  in->orb.e = read_double(infile, byteswap);
  in->orb.x = read_double(infile, byteswap);
  in->orb.w = read_double(infile, byteswap);
  in->orb.t = read_double(infile, byteswap);
  in->orb.pd = read_double(infile, byteswap);
  in->orb.wd = read_double(infile, byteswap);
  in->dms = gen_dvect(in->numdms);
  chkfread(in->dms, sizeof(double), in->numdms, infile);
  in->periods = gen_dvect(in->numperiods);
  chkfread(in->periods, sizeof(double), in->numperiods, infile);
  in->pdots = gen_dvect(in->numpdots);
  chkfread(in->pdots, sizeof(double), in->numpdots, infile);
  in->rawfolds = gen_dvect(in->nsub * in->npart * in->proflen);
  chkfread(in->rawfolds, sizeof(double), in->nsub * 
	   in->npart * in->proflen, infile);
  in->stats = (foldstats *)malloc(sizeof(foldstats) * 
				  in->nsub * in->npart);
  chkfread(in->stats, sizeof(foldstats), 
	   in->nsub * in->npart, infile);
  fclose(infile);
  if (byteswap){
    int ii;
    for (ii=0; ii<in->numdms; ii++)
      in->dms[ii] = swap_double(in->dms[ii]);
    for (ii=0; ii<in->numperiods; ii++)
      in->periods[ii] = swap_double(in->periods[ii]);
    for (ii=0; ii<in->numpdots; ii++)
      in->pdots[ii] = swap_double(in->pdots[ii]);
    for (ii=0; ii<in->nsub*in->npart*in->proflen; ii++)
      in->rawfolds[ii] = swap_double(in->rawfolds[ii]);
    for (ii=0; ii<in->nsub*in->npart; ii++){
      in->stats[ii].numdata  = swap_double(in->stats[ii].numdata);
      in->stats[ii].data_avg = swap_double(in->stats[ii].data_avg);
      in->stats[ii].data_var = swap_double(in->stats[ii].data_var);
      in->stats[ii].numprof  = swap_double(in->stats[ii].numprof);
      in->stats[ii].prof_avg = swap_double(in->stats[ii].prof_avg);
      in->stats[ii].prof_var = swap_double(in->stats[ii].prof_var);
      in->stats[ii].redchi   = swap_double(in->stats[ii].redchi);
    }
  }
}

void delete_prepfoldinfo(prepfoldinfo *in)
/* Free all dynamic arrays in the prepfold array */
{
  free(in->rawfolds);
  if (in->nsub > 1) free(in->dms);
  free(in->periods);
  free(in->pdots);
  free(in->stats);
  free(in->filenm);
  free(in->candnm);
  free(in->telescope);
  free(in->pgdev);
}

void double2float(double *in, float *out, int numpts)
/* Copy a double vector into a float vector */
{
  int ii;

  for (ii = 0; ii < numpts; ii++)
    out[ii] = (float) in[ii];
}
