/* 
   Copyright (C) 2015   Shiro Ikeda <shiro@ism.ac.jp>

   This is file 'mfista.c'. An optimization algorithm for imaging of
   interferometry. The idea of the algorithm was from the following
   two papers,

   Beck and Teboulle (2009) SIAM J. Imaging Sciences, 
   Beck and Teboulle (2009) IEEE trans. on Image Processing 


   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/ 

#include "mfista.h"

void usage(char *s)
{
  printf("%s <int m> <intl n> <V fname> <A fname> <double lambda_l1> <double lambda_tv> <double lambda_tsv> <double c> <X outfile> {X initfile} {-t} {-rec NX} {-nonneg} {-cl_box box_fname} {-log log_fname}\n\n",s);
  printf("  <int m>: number of row of A.\n");
  printf("  <int n>: number of column of A.\n");
  printf("  <V fname>: file name of V.\n");
  printf("  <A fname>: file name of A.\n");
  printf("  <double lambda_l1>: value of lambda_l1. Positive.\n");
  printf("  <double lambda_tv>: value of lambda_tv. Positive.\n");
  printf("  <double lambda_tsv>: value of lambda_tsv. Positive.\n");
  printf("  <double c>: value of c. Positive.\n");
  printf("  <X outfile>: file name to write X.\n\n");

  printf(" Options.\n\n");
    
  printf("  {X initfile}: file name of X for initialization.\n");
  printf("  {-t}: use this if A is stored with row major mode.\n");
  printf("  {-rec NX}: use this if image is not square but rectangular.\n");
  printf("             NX is the length of one dimension of the image.\n");
  printf("  {-maxiter N}: maximum number of iteration.\n");
  printf("  {-eps epsilon}: epsilon used to check the convergence.\n");
  /*  printf("  {-looe}: Compute approximation of LOOE.\n");*/
  printf("  {-nonneg}: Use this if x is nonnegative.\n");
  printf("  {-cl_box box_fname}: file name of CLEAN box data (float).\n");  
  printf("  {-log log_fname}: Specify log file.\n\n");

  printf(" This program solves \n\n");

  printf(" argmin |v-Ax|_2^2/2 + lambda_l1 |x|_1\n\n");
  printf("    or\n\n");
  printf(" argmin |v-Ax|_2^2/2 + lambda_l1 |x|_1 + lambda_tv TV(x)\n\n");
  printf("    or\n\n");
  printf(" argmin |v-Ax|_2^2/2 + lambda_l1 |x|_1 + lambda_tsv TSV(x)\n\n");

  printf(" and write x to <X out file>\n\n");

  printf(" If {-nonneg} option is used, x vector is restricted to be nonnegative.\n\n");

  printf(" c is a parameter used for stepsize. Large c makes the algorithm\n");
  printf(" stable but slow. Around 500000 is fine.\n\n");

  printf(" Files are binary. Read and Write with fread() and fwrite().\n");
  printf(" A is col major. This is C program but blas is based on fortran.\n\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  double *y, *A, *xinit, *xvec, cinit, lambda_l1, lambda_tv, lambda_tsv, eps = EPS;
  char init_fname[1024], box_fname[1024], fname[1024], log_fname[1024];
  int i, M, N, NX, NY, 
    trans_flag = 0, rec_flag = 0, init_flag = 0, box_flag = 0, nonneg_flag = 0, looe_flag = 0,
    log_flag = 0, maxiter = MAXITER;
  unsigned long tmpdnum, dnum;
  struct RESULT    mfista_result;
  struct IO_FNAMES mfista_io;
  FILE* log_fid;

  float *cl_box;

  /* options */

  if (argc<10) usage(argv[0]);

  for(i=10; i<argc ; i++){
    if(strcmp(argv[i],"-t") == 0){
      trans_flag = 1; 
    }
    else if(strcmp(argv[i],"-rec") == 0){
      rec_flag = 1;
      ++i;
      NX = atoi(argv[i]);
    }
    else if(strcmp(argv[i],"-maxiter") == 0){
      ++i;
      maxiter = atoi(argv[i]);
    }
    else if(strcmp(argv[i],"-eps") == 0){
      ++i;
      eps = atof(argv[i]);
    }
    else if(strcmp(argv[i],"-looe") == 0){
      looe_flag = 1; 
    }
    else if(strcmp(argv[i],"-log") == 0){
      log_flag = 1;

      ++i;
      strcpy(log_fname,argv[i]);
    }
    else if(strcmp(argv[i],"-nonneg") == 0){
      nonneg_flag = 1;
    }
    else if(strcmp(argv[i],"-cl_box") == 0){
      box_flag = 1;

      ++i;
      strcpy(box_fname,argv[i]);
    }
    else{
      init_flag = 1;
      strcpy(init_fname,argv[i]);
    }
  }

  M = atoi(argv[1]);
  printf("M is %d\n",M);

  N = atoi(argv[2]);
  printf("N is %d\n",N);

  if(rec_flag ==0) NX = (int)sqrt(N);
  
  printf("NX is %d\n",NX);

  NY = (int) N/NX;

  /* allocate memory space start */ 

  y = alloc_vector(M);
  A = alloc_matrix(M,N);

  xinit = alloc_vector(N);
  xvec  = alloc_vector(N);

  /* initialize xvec */

  if (init_flag ==1){ 

    printf("Initializing x with %s.\n",init_fname);
    dnum = read_V_vector(init_fname, N, xinit);

    if(dnum != N)
      printf("Number of read data is shorter than expected.\n");
  }
  else
    clear_matrix(xinit, N, 1);

  /* read data matrix start */

  sprintf(fname,"%s",argv[3]);
  dnum = read_V_vector(fname, M, y);

  if(dnum != M) 
    printf("Number of read data is shorter than expected in %s.\n",
	   argv[3]);

  sprintf(fname,"%s",argv[4]);
  dnum = read_A_matrix(fname, M, N, A);

  tmpdnum = M;
  tmpdnum *= N;

  if(dnum != tmpdnum){
    printf("Number of read data is shorter than expected in %s.\n",
	   argv[4]);

    printf("expected num is %ld but it only had %ld.\n",
	   tmpdnum,dnum);
  }

  lambda_l1 = atof(argv[5]);
  printf("lambda_l1 = %g\n",lambda_l1);

  lambda_tv = atof(argv[6]);
  printf("lambda_tv = %g\n",lambda_tv);

  lambda_tsv = atof(argv[7]);
  printf("lambda_tsv = %g\n",lambda_tsv);

  cinit = atof(argv[8]);
  printf("c = %g\n",cinit);

  if (nonneg_flag == 1)
    printf("x is nonnegative.\n");

  if (log_flag ==1)
    printf("Log will be saved to %s.\n",log_fname);

  if (looe_flag ==1)
    printf("Approximation of LOOE will be computed.\n\n");
  else
    printf("\n");

  cl_box = alloc_f_vector(N);
  
  if (trans_flag ==1){ 
    transpose_matrix(A, N, M);
  }

  /* main calculation */
  
  mfista_imaging_core_dft(y, A, &M, &N, NX, NY, maxiter, eps, lambda_l1, lambda_tv, lambda_tsv,
			  cinit, xinit, xvec, nonneg_flag, looe_flag, box_flag, cl_box,
			  &mfista_result);

  /* post processing */

  write_X_vector(argv[9], N, xvec);

  mfista_io.fft      = 0;
  mfista_io.fft_fname = NULL;
  mfista_io.v_fname   = argv[3];
  mfista_io.A_fname   = argv[4];

  if(init_flag == 1)
    mfista_io.in_fname = init_fname;
  else
    mfista_io.in_fname = NULL;

  mfista_io.out_fname = argv[9];
  show_io_fnames(stdout, argv[0], &mfista_io);
  show_result(stdout, argv[0], &mfista_result);

  if(log_flag == 1){
    log_fid = fopenw(log_fname);
    show_io_fnames(log_fid, argv[0], &mfista_io);
    show_result(log_fid, argv[0], &mfista_result);
    fclose(log_fid);
  }

  /* clear memory */

  free(y);
  free(A);
  free(xvec);

  return(0);
}
