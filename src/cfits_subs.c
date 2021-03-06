#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* FITS FILE INTERACTION ROUTINES */

/* $Id: cfits_subs.c,v 1.8 2004/06/10 04:13:48 wwilson Exp $ */

/* This version for CFITSIO */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cor_common.h"
#include "product_types.h"
#include "obs_types.h"
#include "scan_header.h"
#include "supplhdr.h"
#include "newcycle.h"
#include "oldcycle.h"

#include "cordat.h"

/* cfitsio include file */
#include <fitsio.h>

#define WBSAM_FIXED 0x2

/* Define CFITS file pointer - global- defined in fitsio.h */
fitsfile *cfitsptr;

int new_file = 0;

/* hdu numbers */
int dig_stats_hdu;
int bpass_hdu;
int last_scanhdr_hdu;
int subint_hdu;

int bat_to_ut_secs( double *ut_secs, long long bat );
int bat_to_ut_str( char *str, int len, long long bat );
int bat_to_mjd( double *mjd, long long bat );
long long BAT( void );
void rads_to_str( char *str, int str_len, double rads, int mode );


/* BEGIN */

int CDfits_open_file( char *filename )
{
  int status;
  int bitpix   =  8;
  int naxis   =  0;
  long *naxes;
  
  status = 0;
  /* Open file */
  fits_create_file( &cfitsptr, filename, &status );
  /* Write primary header */
  bitpix = 8;
  naxis = 0;
  fits_create_img( cfitsptr, bitpix, naxis, naxes, &status );

  /* Add first keywords */
  fits_update_key( cfitsptr, TSTRING, "HDRVER", "1.19", "Header version", &status);
  fits_write_date( cfitsptr, &status);

  new_file = 1;
  
  
  return( status );
  
}


int CDfits_write_scanhdr( void )
{
  int status;
  int i, j, k, m, n;
  short int sj;
  
  double dx, dy, dz;
  
  long lk;
    
  int nrows, ncols;
  long naxes[3];

  char *ttype[4], *tform[4], *tunit[4];

  char str[16], Istr[16], *qch, *rch, *sch, *tch;

  int chan_inc, first_normal_group, npols, pol;
      
  float x;

#undef POINT_PAR
#ifdef POINT_PAR

  float xa[12];

#endif
  
  FILE *fp;
  char filename[80];
  
#define GSTR_LEN 88
  char gstr[GSTR_LEN];
  char astr[80];
  
  void *pv;

  char date_time[24];

#define MAX_BLKS 20
  char *pch[MAX_BLKS];
  char site[MAX_BLKS][2];
  short int nspan[MAX_BLKS];
  short int ncoeff[MAX_BLKS];
  double rfreq[MAX_BLKS];
  double rmjd[MAX_BLKS];
  double rphase[MAX_BLKS];
  double lgfiterr[MAX_BLKS];
  double f0[MAX_BLKS];
#define MAX_COEFF 15
  double coeff[MAX_BLKS][MAX_COEFF];


  /* For Pulsar History BINTABLE */
 
  static char *PHtype[18] = {
    "DATE_PRO","PROC_CMD","POL_TYPE","NPOL    ","NBIN    ","NBIN_PRD","TBIN    ","CTR_FREQ",
    "NCHAN   ","CHAN_BW ","PAR_CORR","RM_CORR ","DEDISP  ","DDS_MTHD","SC_MTHD ","CAL_MTHD",
    "CAL_FILE","RFI_MTHD"
  };
  
  static char *PHform[18] = {
    "24A     ","80A     ","8A      ","1I      ","1I      ","1I      ","1D      ","1D      ",
    "1I      ","1D      ","1I      ","1I      ","1I      ","32A     ","32A     ","32A     ",
    "32A     ","32A     "
  };
  
  static char *PHunit[18] = {
    "        ","        ","        ","        ","        ","        ","s       ","MHz     ",
    "        ","MHz     ","        ","        ","        ","        ","        ","        ",
    "        ","        "
  };


  /* For Pulsar Epmemeris BINTABLE */

  static char *PEtype[56] = {
    "DATE_PRO", "PROC_CMD", "EPHVER", "PSR_NAME", "RAJ", "DECJ", "PMRA", "PMDEC", 
    "PX", "POSEPOCH", "IF0", "FF0", "F1", "F2", "F3", "PEPOCH",
    "DM", "DM1", "RM", "BINARY", "T0", "PB", "A1", "OM", 
    "OMDOT", "ECC", "PBDOT", "GAMMA", "SINI", "M2", "T0_2", "PB_2",
    "A1_2", "OM_2", "ECC_2", "DTHETA", "XDOT", "EDOT", "TASC", "EPS1",
    "EPS2", "START", "FINISH", "TRES", "NTOA", "CLK", "EPHEM", "TZRIMJD",
    "TZRFMJD", "TZRFRQ", "TZRSITE", "GLEP_1", "GLPH_1", "GLF0_1", "GLF1_1", "GLF0D_1"
  };
  
  static char *PEform[56] = {
    "24A     ", "80A     ", "16A     ", "16A     ", "24A     ", "24A     ", "1D      ", "1D      ",
    "1D      ", "1D      ", "1J      ", "1D      ", "1D      ", "1D      ", "1D      ", "1D      ",
    "1D      ", "1D      ", "1D      ", "8A      ", "1D      ", "1D      ", "1D      ", "1D      ",
    "1D      ", "1D      ", "1D      ", "1D      ", "1D      ", "1D      ", "1D      ", "1D      ",
    "1D      ", "1D      ", "1D      ", "1D      ", "1D      ", "1D      ", "1D      ", "1D      ",
    "1D      ", "1D      ", "1D      ", "1D      ", "1I      ", "12A     ", "12A     ", "1J      ",
    "1D      ", "1D      ", "1A      ", "1D      ", "1D      ", "1D      ", "1D      ", "1D      "
  };
  
  /* For Pulsar Polyco BINTABLE */

  static char *PPtype[13] = {
    "DATE_PRO", "POLYVER ", "NSPAN   ", "NCOEF   ", "NPBLK   ", "NSITE   ", "REF_FREQ", "PRED_PHS",
    "REF_MJD ", "REF_PHS ", "REF_F0  ", "LGFITERR", "COEFF   "
  };
    
  static char *PPform[13] = {
    "24A     ", "16A     ", "1I      ", "1I      ", "1I      ", "1A      ", "1D      ", "1D      ",
    "1D      ", "1D      ", "1D      ", "1D      ", "15D     ",
  };

  /* BEGIN */

  status = 0;

  /* Get DateTime string - required in various BINTABLEs */
  fits_get_system_time( date_time, &n, &status );
  
  if( new_file ) {

    /* Add required keywords to primary header */

    /* fits_update_key( cfitsptr, TSTRING, "ORIGIN", "RNM", 
       "Output class", &status); */

    fits_update_key( cfitsptr, TSTRING, "OBSERVER", header.observer, 
		     "Observer name(s)", &status);
    fits_update_key( cfitsptr, TSTRING, "PROJID", supplhdr.project, 
		     "Project name", &status);
    fits_update_key( cfitsptr, TSTRING, "TELESCOP", supplhdr.site_name,
		     "Telescope name", &status);
    fits_update_key( cfitsptr, TDOUBLE, "ANT_X", &header.antenna[0].x,
		     "[m] Antenna ITRF X-coordinate (D)", &status);
    fits_update_key( cfitsptr, TDOUBLE, "ANT_Y", &header.antenna[0].y,
		     "[m] Antenna ITRF Y-coordinate (D)", &status);
    fits_update_key( cfitsptr, TDOUBLE, "ANT_Z", &header.antenna[0].z,
		     "[m] Antenna ITRF Z-coordinate (D)", &status);
    fits_update_key( cfitsptr, TSTRING, "FRONTEND", supplhdr.frontend, 
		     "Rx and feed ID", &status);

    fits_update_key( cfitsptr, TSTRING, "FD_POLN", supplhdr.polzn,
		     "LIN or CIRC", &status);
    x = 0.0;
    fits_update_key( cfitsptr, TFLOAT, "XPOL_ANG", &x,
		     "[deg] Angle of X-probe wrt platform zero (E)", &status);

    fits_update_key( cfitsptr, TSTRING, "BACKEND", supplhdr.backend, 
		     "Backend ID", &status);
    fits_update_key( cfitsptr, TSTRING, "BECONFIG", (*c_c).config_menu_name,
		     "Backend configuration file name", &status);

    /* Basic cycle time for integration */
    dx = (*c_c).integ_time / 1.0E06;
    fits_update_key( cfitsptr, TDOUBLE, "TCYCLE", &dx,
		     "[s] Correlator cycle time (D)", &status);

    /* Find the first normal group in band */
    first_normal_group = -1;
    i = (*c_c).band[0].first_group;
    k = i + (*c_c).band[0].num_group - 1;
    while( ( i <= k ) && ( first_normal_group < 0 ) ) {
      if( (*c_c).product[ (*c_c).prod_group[i].first_prod ].type < POL_CAL_OFF ) {
	first_normal_group = i;
      }
      i++;
    }
    if( first_normal_group < 0 ) first_normal_group = 0;

    npols = (*c_c).prod_group[first_normal_group].num_prod;

    /* Receiver channels - best guess */
    if( npols == 1 ) nrcvr = 1;
    else nrcvr = 2;
    
    fits_update_key( cfitsptr, TINT, "NRCVR", &nrcvr,
		     "Number of receiver channels (I)", &status);
    fits_update_key( cfitsptr, TSTRING, "OBS_MODE", supplhdr.obsmode,
		     "(PSR, CAL, SEARCH)", &status);
    /* fits_update_key( cfitsptr, TSTRING, "DATATYPE", "FOLD",
                     "FOLD or DUMP", &status);  */
    fits_update_key( cfitsptr, TSTRING, "SRC_NAME", header.source,
		     "Source or scan ID", &status);
    fits_update_key( cfitsptr, TSTRING, "COORD_MD", header.epoch,
		     "Coordinate mode (J2000, Gal, Ecliptic, etc.)", &status);

    /* J2000 or B1950 */
    if( header.epoch[0] == 'J' || header.epoch[0] == 'B') j = 0;
    /* All others */
    else j = 2;

    rads_to_str( gstr, 16, supplhdr.start_long, j );

    /*
    gstr[17] = '\0';
    printf("\n rads_to_str(mode0) on %f = '%s'", supplhdr.start_long, gstr );
    */

    fits_update_key( cfitsptr, TSTRING, "STT_CRD1", gstr,
		     "Start coord 1 (hh:mm:ss.sss or ddd.ddd)", &status);

    rads_to_str( gstr, 16, supplhdr.start_lat, j+1 );
    fits_update_key( cfitsptr, TSTRING, "STT_CRD2", gstr,
		     "Start coord 2 (-dd:mm:ss.sss or -dd.ddd)", &status);
   
    fits_update_key( cfitsptr, TSTRING, "TRK_MODE", supplhdr.trackmode,
		     "Track mode (TRACK, SCANGC, SCANLAT)", &status);

    rads_to_str( gstr, 16, supplhdr.stop_long, j );
    fits_update_key( cfitsptr, TSTRING, "STP_CRD1", gstr,
		     "Stop coord 1 (hh:mm:ss.sss or ddd.ddd)", &status);

    rads_to_str( gstr, 16, supplhdr.stop_lat, j+1 );
    fits_update_key( cfitsptr, TSTRING, "STP_CRD2", gstr,
		     "Stop coord 2 (-dd:mm:ss.sss or -dd.ddd)", &status);
   
    x = supplhdr.scan_cycles * ( (*c_c).integ_time / 1.0E06 );
    fits_update_key( cfitsptr, TFLOAT, "SCANLEN ", &x,
		     "[s] Requested scan length (E)", &status);
    
    fits_update_key( cfitsptr, TSTRING, "FD_MODE", supplhdr.fdmode,
		     "Feed track mode - Const FA, CPA, GPA", &status);

    fits_update_key( cfitsptr, TFLOAT, "FA_REQ", &supplhdr.xpolang,
		     "[deg] Feed/Posn angle requested (E)", &status);

    fits_update_key( cfitsptr, TFLOAT, "ATTEN_A", &supplhdr.atten_a,
		     "[db] Attenuator, Poln A (E)", &status);

    fits_update_key( cfitsptr, TFLOAT, "ATTEN_B", &supplhdr.atten_b,
		     "[db] Attenuator, Poln B (E)", &status);

    fits_update_key( cfitsptr, TSTRING, "CAL_MODE", supplhdr.calmode,
		     "Cal mode (OFF, SYNC, EXT1, EXT2)", &status);

    x = (float) (*c_c).pulsar_ncal_sig_frequency;
    fits_update_key( cfitsptr, TFLOAT, "CAL_FREQ", &x,
		     "[Hz] Cal modulation frequency (E)", &status);

    x = (float) (*c_c).pulsar_ncal_sig_duty_cycle;
    fits_update_key( cfitsptr, TFLOAT, "CAL_DCYC", &x,
		     "Cal duty cycle (E)", &status);

    x = (float) (*c_c).pulsar_ncal_sig_start_phase;
    fits_update_key( cfitsptr, TFLOAT, "CAL_PHS ", &x,
		     "Cal phase (wrt start time) (E)", &status);
    

    new_file = 0;

  }

#ifdef POINT_PAR

  /* Add Pointing Parameters BINTABLE */

  nrows = 0;  /* naxis2 - Let CFITSIO sort this out */
  ncols = 1; /* tfields */
  ttype[0] = "PPAR    ";
  tform[0] = "12E     ";

  fits_create_tbl( cfitsptr, BINARY_TBL, nrows, ncols, 
		   ttype, tform, NULL, "POINTPAR", &status);

  /* Pointing parameters */
  for( n=0; n<11; n++ ) xa[n] = (float) supplhdr.pointing_parameter[n];
  xa[11] = 0.0;
  fits_write_col( cfitsptr, TFLOAT, 1, 1, 1, 12, xa, &status );

#endif

  /* Add Processing History BINTABLE */

  nrows = 0;  /* naxis2 - Let CFITSIO sort this out */
  ncols = 18; /* tfields */
   
  fits_create_tbl( cfitsptr, BINARY_TBL, nrows, ncols, 
		   PHtype, PHform, PHunit, "HISTORY", &status);

  /* Processing date and time (YYYY-MM-DDThh:mm:ss UTC) */
  pch[0] = date_time;
  fits_write_col( cfitsptr, TSTRING, 1, 1, 1, 1, pch, &status );
  /* Processing program and command */
  pch[0] = "WBCOR";
  fits_write_col( cfitsptr, TSTRING, 2, 1, 1, 1, pch, &status );
  /* Polarisation identifier */
  if( npols < 4 ) {
    str[0] = '\0';
    k = (*c_c).prod_group[first_normal_group].first_prod;
    m = 0;
    for( j=0; j<npols; j++ ) {
      pol = (*c_c).product[k].polarisation;
      if( pol == 1 ) strcat( str, "XX" );
      else if( pol == 2 ) {
	/* An XY product will be written out as two pols, real and imag */
	strcat( str, "CRCI" );
	m++;
      }
      else if( pol == 3 ) {
	/* A YX product will be written out as two pols, real and imag */
	strcat( str, "CRCI" );
	m++;
      }
      else if( pol == 4 ) strcat( str, "YY" );
      else strcat( str, "??" );
      k++;
    }
  }
  else {
    strcpy( str, "XXYYXYYX" );
  }
  pch[0] = str;
  fits_write_col( cfitsptr, TSTRING, 3, 1, 1, 1, pch, &status );

  /* Nr of pols  - i.e. actually written out */
  sj = npols + m;
  fits_write_col( cfitsptr, TSHORT, 4, 1, 1, 1, &sj, &status );

  /* Nr of bins per product (0 for SEARCH mode) */
  sj = (*c_c).band[0].num_bins;
  fits_write_col( cfitsptr, TSHORT, 5, 1, 1, 1, &sj, &status );

  /* Nr of bins per period */
  fits_write_col( cfitsptr, TSHORT, 6, 1, 1, 1, &sj, &status );

  /* Bin time */
  dx =  (*c_c).pulsar_period / sj;
  fits_write_col( cfitsptr, TDOUBLE, 7, 1, 1, 1, &dx, &status );

  /* Centre freq. */
  dx = header.band[0].frequency;  
  fits_write_col( cfitsptr, TDOUBLE, 8, 1, 1, 1, &dx, &status );

  /* Number of channels */
  chan_inc = header.band[0].channel_increment;
  if( chan_inc <= 0 ) chan_inc = 1;
  n = ( ( header.band[0].last_channel - 
		   header.band[0].first_channel ) 
		 / chan_inc ) + 1;

  /* Don't include DC channel */  
  if( header.band[0].first_channel == 1 ) n--;
  
  sj = n;
  fits_write_col( cfitsptr, TSHORT, 9, 1, 1, 1, &sj, &status );

  /* Channel bandwidth */
  dy = header.band[0].bandwidth / (double) ( (*c_c).band[0].nfft / 2 );
  if( header.band[0].spectrum_inverted < 0 ) dy = -dy;

  /* Channel incr. width - signed */
  dx = (double) chan_inc * dy;
  fits_write_col( cfitsptr, TDOUBLE, 10, 1, 1, 1, &dx, &status );

  /* Create frequency array for later */
  /* Get freq. of first channel */
  if( ( k = header.band[0].first_channel ) == 1 ) k = 2;
  
  j = ( (*c_c).band[0].nfft / 4 ) + 1 - k;
  dz = header.band[0].frequency - ( j * dy );

  for(i=0; i<n; i++ ) {
    binned_freq[i] = (float) dz;
    dz += dx;
  }    


  sj = 0;
  /* Parallactic angle correction applied */
  fits_write_col( cfitsptr, TSHORT, 11, 1, 1, 1, &sj, &status );
  /* RM correction applied */
  fits_write_col( cfitsptr, TSHORT, 12, 1, 1, 1, &sj, &status );
  /* Data dedispersed */
  fits_write_col( cfitsptr, TSHORT, 13, 1, 1, 1, &sj, &status );
  /* Dedispersion method */
  pch[0] = "NONE";
  fits_write_col( cfitsptr, TSTRING, 14, 1, 1, 1, pch, &status );
  /* Scattered power correction method */
  pch[0] = supplhdr.scpwr;
  fits_write_col( cfitsptr, TSTRING, 15, 1, 1, 1, pch, &status );
  /* Calibration method */
  pch[0] = "NONE";
  fits_write_col( cfitsptr, TSTRING, 16, 1, 1, 1, pch, &status );
  /* Name of calibration file */
  pch[0] = "NONE";
  fits_write_col( cfitsptr, TSTRING, 17, 1, 1, 1, pch, &status );
  /* RFI excision method */
  pch[0] = supplhdr.rfiex;
  fits_write_col( cfitsptr, TSTRING, 18, 1, 1, 1, pch, &status );


  /* Add Original BANDPASS BINTABLE */

  nrows = 0;  /* naxis2 - Let CFITSIO sort this out */
  ncols = 3;  /* tfields */

  nchan_orig = ( (*c_c).band[0].nfft / 2 ) + 1;

  naxes[0] = nchan_orig;
  naxes[1] = nrcvr;

  sprintf( str, "%ldE", naxes[1] );

  ttype[0] = "DAT_OFFS";
  tform[0] = str;
  tunit[0] = "";

  ttype[1] = "DAT_SCL ";
  tform[1] = str;
  tunit[1] = "";

  sprintf( Istr, "%ldI", ( naxes[0] * naxes[1] ) );
  ttype[2] = "DATA    ";
  tform[2] = Istr;
  tunit[2] = "Jy      ";

  fits_create_tbl( cfitsptr, BINARY_TBL, nrows, ncols, 
		   ttype, tform, tunit, "BANDPASS", &status);

  /* Add dimensions of column 3 = BANDPASS Data */  
  fits_write_tdim( cfitsptr, 3, 2, naxes, &status );

  /* Add keywords */
  fits_update_key( cfitsptr, TINT, "NCH_ORIG", &nchan_orig,
		     "Number of channels in original bandpass (I)", &status);
  fits_update_key( cfitsptr, TINT, "BP_NPOL ", &nrcvr,
		     "Number of polarizations in bandpass (I)", &status);

  /* Store hdu number */
  fits_get_hdu_num( cfitsptr, &bpass_hdu );


  /* Add Pulsar Ephemeris BINTABLE */

  nrows = 0;  /* naxis2 - Let CFITSIO sort this out */
  ncols = 56; /* tfields */

  fits_create_tbl( cfitsptr, BINARY_TBL, nrows, ncols, 
		   PEtype, PEform, NULL, "PSREPHEM", &status);

  /* Processing date and time (YYYY-MM-DDThh:mm:ss UTC) */
  pch[0] = date_time;
  fits_write_col( cfitsptr, TSTRING, 1, 1, 1, 1, pch, &status );
  /* Processing program and command */
  pch[0] = "WBCOR";
  fits_write_col( cfitsptr, TSTRING, 2, 1, 1, 1, pch, &status );
  /* Ephemeris version */
  pch[0] = "PSRINFO:1.4";
  fits_write_col( cfitsptr, TSTRING, 3, 1, 1, 1, pch, &status );

  /* Open ephemeris file */
  /* Construct filename  */
  if( ( qch = getenv( "cor_pulsar" ) ) == NULL ) return(-11);

  strcpy( filename, qch );
  if( filename[ strlen(filename) - 1 ] != '/' )
    strcat( filename, "/" );
  strcat( filename, "online.eph" );
 
  if( ( fp = fopen( filename, "r" ) ) == NULL ) return(-12);

  while( fgets( gstr, GSTR_LEN, fp ) != NULL ) {

    if( ( qch = strtok( gstr, " " ) ) != NULL ) {

      /* Following gets a name change - From PSRJ to PSR_NAME */
      if( strcmp( qch, "PSRJ" ) == 0 ) i = 3;
      /* Following two get split into integer and fractional parts */
      else if( strcmp( qch, "F" ) == 0 ) i = 10;
      else if( strcmp( qch, "F0" ) == 0 ) i = 10; /* Possible alternative to just F */
      else if( strcmp( qch, "TZRMJD" ) == 0 ) i = 47;
      else {
	for( i=0; i<ncols; i++ ) {
	  if( strcmp( qch, PEtype[i] ) == 0 ) break;
	}
      }

      if( i < ncols ) {
	/* Get next parameter */
	if( ( rch = strtok( NULL, " " ) ) == NULL ) return(-13);

	if( i == 10 || i == 47 || ( qch = strchr( PEform[i], 'D' ) ) != NULL ) {
	  /* Change any D exponent to E */
	  if( ( sch = strrchr( rch, 'D' ) ) != NULL ) *sch = 'E';
	  tch = sch;
	
	  if( i == 10 || i == 47 ) {
	    /* Convert to integer + fractional part AND
	       if i == 10 convert from Hz to mHz i.e. multiply by 1000 */
	    /* Get exponent */
	    if( sch != NULL ) {
	      if( sscanf( ++sch, "%d", &j ) != 1 )  return(-91);
	    }
	    else j = 0;
	  
	    /* get position of decimal point */
	    if( ( sch = strrchr( rch, '.' ) ) == NULL ) {
	      /* Integer part only */
	      if( sscanf( rch, "%ld", &lk ) != 1 ) return(-92);
	      dx = 0.0;
	    }
	    else {
	      /* Decimal point present */
	      if( i == 10 ) j += 3;  /* multiply by 1000 - sec. to ms. */

	      if( j >= 0 ) {
		k = (int) ( sch - rch );
		if( k > 0 ) {
		  strncpy( astr, rch, k );
		  astr[k] = '\0';
		}
		else strcpy( astr, "0" );
	      
		if( j > 0 ) { /* multiply by 10**j, i.e. move decimal point */
		  strncat( astr, (sch+1), j );
		  *(sch += j) = '.';
		  *--sch = '0';
		}
		/* Now astr contains integer part, sch points to fractional part */
		if( sscanf( astr, "%ld", &lk ) != 1 ) return(-93);
		/* Don't want exponent - reduced to fraction */
		if( tch != NULL ) *tch = '\0';
		if( sscanf( sch, "%lf", &dx ) != 1 ) return(-94);
	      }
	      else {
		/* Fractional part only */
		lk = 0;
		if( sscanf( sch, "%lf", &dx ) != 1 ) return(-94);
		if( i == 10 ) dx *= 1000.0;
	      }
	    	    
	    }
	  
	    pv = &lk;
	    j = TLONG;

	  }
	  else {

	    if( sscanf( rch, "%lf", &dx ) != 1 ) return(-14);

	    j = TDOUBLE;
	    pv = &dx;

	  }	
	}
	else if( ( qch = strchr( PEform[i], 'A' ) ) != NULL ) {
	  j = TSTRING;
	  /* Strip off any trailing newline */
	  if( ( sch = strrchr( rch, '\n' ) ) != NULL ) *sch = '\0';
	  pch[0] = rch;
	  pv = pch;
	}
	else if( ( qch = strchr( PEform[i], 'I' ) ) != NULL ) {
	  j = TSHORT;
	  if( sscanf( rch, "%d", &k ) != 1 ) return(-15);
	  sj = k;
	  pv = &sj;
	}
	else if( ( qch = strchr( PEform[i], 'J' ) ) != NULL ) {
	  j = TINT;
	  if( sscanf( rch, "%d", &k ) != 1 ) return(-16);
	  pv = &k;
	}
	else return(-17);
            
	fits_write_col( cfitsptr, j, i+1, 1, 1, 1, pv, &status );

	/* Write fractional part */
	if( i == 10 || i == 47 ) {
	  j = TDOUBLE;
	  pv = &dx;
	  fits_write_col( cfitsptr, j, i+2, 1, 1, 1, pv, &status );
	  
	  printf( "\n In Ephemeris BINTABLE: %s = %ld %e", PEtype[i], lk, dx );
	}
      }
    }
  }
  fclose( fp );
  
  /* Add Digitiser Statistics BINTABLE */

  nrows = 0;  /* naxis2 - Let CFITSIO sort this out */
  ncols = 1; /* tfields */

  wbsam_levs = 2;
  num_wbsams = nrcvr;

  naxes[0] = wbsam_levs;
  naxes[1] = num_wbsams;
  if( ( n = header.cycles_to_avg ) <= 0 ) n = 1;
  naxes[2] = n;

  sprintf( str, "%ldE", ( naxes[0] * naxes[1] * naxes[2] ) );
  ttype[0] = "DATA    ";
  tform[0] = str;

  fits_create_tbl( cfitsptr, BINARY_TBL, nrows, ncols, 
		   ttype, tform, NULL, "DIG_STAT", &status);

  /* Add dimensions of column 1 = Data */  
  fits_write_tdim( cfitsptr, 1, 3, naxes, &status );

  /* Add keywords */
  fits_update_key( cfitsptr, TSTRING, "DIG_MODE", "2-bit,3-level",
		     "Digitiser mode", &status);
  fits_update_key( cfitsptr, TINT, "NDIGR", &num_wbsams,
		     "Number of digitised channels (I)", &status);
  fits_update_key( cfitsptr, TINT, "NLEV", &wbsam_levs,
		     "Number of digitiser levels (I)", &status);
  fits_update_key( cfitsptr, TINT, "NCYCSUB", &n,
		     "Number of correlator cycles per subint (I)", &status);

  /* Store current hdu number */
  fits_get_hdu_num( cfitsptr, &dig_stats_hdu );


  
  /* Add Polyco History BINTABLE */

  /* Read in polyco file */
  /* Construct filename from .eph name  */

  qch = strstr( filename, ".eph" );
  *qch = '\0';
  strcat( filename, ".polyco" );
  if( ( fp = fopen( filename, "r" ) ) == NULL ) return(-18);

  k = 0;
  while( ( k < MAX_BLKS ) && 
	 ( ( n = fscanf( fp, "%s %s%lf%lf%lf%lf%lf%lf%lf %s%hd%hd%lf%16c", 
			 gstr, gstr, &dx, &rmjd[k], &dx, &dx, &lgfiterr[k], &rphase[k], &f0[k],
			 site[k], &nspan[k], &ncoeff[k], &rfreq[k], gstr ) ) != EOF ) ) {
    if( n != 14 ) return(-20);
    if( ncoeff[k] > MAX_COEFF ) return(-21);
    
    for( i=0; i<ncoeff[k]; i++ ) {
      if( fscanf( fp, "%s", gstr  ) != 1 ) return(-22);
      /* Translate exponent D to E */
      if( ( qch = strchr( gstr, (int) 'D' ) ) != NULL ) *qch = 'E';
      if( sscanf( gstr, "%lE", &coeff[k][i] ) != 1 ) return(-23);
    }
    for( i=ncoeff[k]; i<MAX_COEFF; i++ ) coeff[k][i] = 0.0;
    k++;
  }
  if( k >= MAX_BLKS ) return(-24);
  
  fclose( fp );

  printf("\nCFITS_SUBS:  Polyco - Site[0,1](hex) = %x %x", site[0][0], site[0][1] );
  

  nrows = 0;  /* naxis2 - Let CFITSIO sort this out */
  ncols = 13; /* tfields */
  

  fits_create_tbl( cfitsptr, BINARY_TBL, nrows, ncols, 
		   PPtype, PPform, NULL, "POLYCO  ", &status);

  /* Processing date and time (YYYY-MM-DDThh:mm:ss UTC) */
  for( i=0; i<k; i++ ) pch[i] = date_time;
  fits_write_col( cfitsptr, TSTRING, 1, 1, 1, k, pch, &status );
  /* Polyco version */
  for( i=0; i<k; i++ ) pch[i] = "TEMPO:11.0";
  fits_write_col( cfitsptr, TSTRING, 2, 1, 1, k, pch, &status );
  /* Span of polyco in min */
  fits_write_col( cfitsptr, TSHORT, 3, 1, 1, k, nspan, &status );
  /* Nr of coefficients - per block */
  fits_write_col( cfitsptr, TSHORT, 4, 1, 1, k, ncoeff, &status );
  /* Nr of polyco blocks (of NCOEF coefficients) -reuse nspan array */
  for( i=0; i<k; i++ ) nspan[i] = k;
  fits_write_col( cfitsptr, TSHORT, 5, 1, 1, k, nspan, &status );
  /* TEMPO site code - 1 character */
  for( i=0; i<k; i++ ) pch[i] = &site[i][0];
  fits_write_col( cfitsptr, TSTRING, 6, 1, 1, k, pch, &status );
  /* Reference frequency for phase */
  fits_write_col( cfitsptr, TDOUBLE, 7, 1, 1, k, rfreq, &status );

  /* Predicted pulse phase at obs start - reuse rfreq  */
  for( i=0; i<k; i++ ) rfreq[i] = newcycle.WBpsr_timer_start_phase;
  fits_write_col( cfitsptr, TDOUBLE, 8, 1, 1, k, rfreq, &status );

  /* Reference MJD - NPBLK doubles */
  fits_write_col( cfitsptr, TDOUBLE, 9, 1, 1, k, rmjd, &status );
  /* Reference phase - NPBLK doubles */
  fits_write_col( cfitsptr, TDOUBLE, 10, 1, 1, k, rphase, &status );
  /* Reference F0 - NPBLK doubles */
  fits_write_col( cfitsptr, TDOUBLE, 11, 1, 1, k, f0, &status );
  /* Fit error - NPBLK doubles */
  fits_write_col( cfitsptr, TDOUBLE, 12, 1, 1, k, lgfiterr, &status );
  /* Polyco coefficients - NPBLK*NCOEF doubles */
  j = k * MAX_COEFF;
  fits_write_col( cfitsptr, TDOUBLE, 13, 1, 1, j, &coeff[0][0], &status );


  /* Store last header hdu number */
  fits_get_hdu_num( cfitsptr, &last_scanhdr_hdu );
  
  /* Restart SUBINT counter */
  subint_cnt = 0;
  
  return( status );

}



void CDfits_fill_scanhdr( double *ut_secs )
{
  
}


int CDfits_fill_mosaic_list( char *buf, char *errstr )
{

  return(0);
  
}



int CDfits_write_data( __complex__ float *data, int xant, int yant,
		     float ut_secs, float u, float v, float w,
		     int flag, int bin_num, int if_number, 
		     int source_number, float integration_time )
{
  return( 0 );
  
}



int CDfits_write_syscal( float ut_secs, int band_num, int source_num )
{

  return( 0 );
  
}

int CDfits_write_subint( int num_cycles, int num_bins, int num_chans, int num_pols )
{
  int status;
  int j, k;
  int nscl, ndata;
  float x;
  double dx, dy, dz, ds, dc;
    
  int nrows, ncols, col;
  char *ttype[19], *tform[19], *tunit[19];
  long naxes[3];
  
  char Cstr16[16], Estr16[16], Istr16[16];

#define ASTR_LEN 32
  char astr[ ASTR_LEN ];

  
  /* BEGIN */

  nscl =  num_chans * num_pols;
  ndata = num_bins * nscl;

  status = 0;
  
  /* Increment SUBINT counter and if first SUBINT, write header */
  if( ++subint_cnt == 1 ) {

    /* Add START TIME to primary HDU */
    /* Move to primary HDU */
    fits_movabs_hdu( cfitsptr, 1, NULL, &status );

    if( bat_to_ut_str( astr, ASTR_LEN, newcycle.WBpsr_timer_start_bat ) < 0 ) {
      return( -9999 );
    }
    
    astr[10] = '\0';
    fits_update_key( cfitsptr, TSTRING, "STT_DATE", astr,
		     "Start UT date (YYYY-MM-DD)", &status);
    fits_update_key( cfitsptr, TSTRING, "STT_TIME", &astr[11],
		     "Start UT (hh:mm:ss)", &status);

    bat_to_mjd( &dx, newcycle.WBpsr_timer_start_bat );

    dz = modf( dx, &dy );   /* dz = fractional day,  dy = days */    

    j = (int) floor( ( dy + 0.1 ) );  /* Add 0.1d to make sure we get the correct one */
    fits_update_key( cfitsptr, TINT, "STT_IMJD", &j,
		     "Start MJD (UTC days) (J)", &status);
    
    dy = dz * 86400.0;   /* This should be an exact second - timer starts on 1pps */
    k = (int) floor( ( dy + 0.1 ) );   /* Add 0.1s to make sure we get the correct one */
    fits_update_key( cfitsptr, TINT, "STT_SMJD", &k,
		     "[s] Start time (sec past UTC 00h) (J)", &status);

    /* Add in offset = 1 - start phase - one bin !!!!!!!!!!! */
    /* 9 Jun 2004 - remove one bin offset - wrong !! */
    /* Was:
       dz = 1.0 - modf( newcycle.WBpsr_start_psr_phase, &dx ) - 1.0 / (double) num_bins; 
       dz *= (*c_c).pulsar_period;
       Now: the offset time is available directly */
    dz = newcycle.WBpsr_timer_start_offset_secs;
    
    /* Quantise to 128MHz clock cycles */
    dz *= 128.0E06;
    dx = floor( dz );
    dx /= 128.0E06;
    
    fits_update_key( cfitsptr, TDOUBLE, "STT_OFFS", &dx,
		     "[s] Start time offset (D)", &status);

    /* LST provided corresponds to integ. start time, i.e. oldcycle.bat */
    bat_to_ut_secs( &dy, oldcycle.bat );
    dy += dx;
    dx = scan_start_time_secs - dy;
    if( dx < -43200.0 ) dx += 86400.0;
    if( dx >  43200.0 ) dx -= 86400.0;
    dy = ( ( oldcycle.lst * 86400.0 ) / TwoPi ) + ( dx * 1.002737909350795 );
    if( dy < 0.0 ) dy += 86400.0;
    if( dy > 86400.0 ) dy -= 86400.0;
    fits_update_key( cfitsptr, TDOUBLE, "STT_LST ", &dy,
		     "[s] Start LST (D)", &status);

    /* Finished with primary HDU */

    /* Move to last created HDU in scan header */
    fits_movabs_hdu( cfitsptr, last_scanhdr_hdu, NULL, &status );
    

    /* Create SUBINT BINTABLE */

    nrows = 0; /* naxis2 - Let CFITSIO sort this out */
    ncols = 19; /* tfields */

    ttype[0] = "ISUBINT ";    /* Subint number. If NAXIS=-1, 0 indicates EOD. */
    tform[0] = "1J      ";
    tunit[0] = "";
    ttype[1] = "INDEXVAL";    /* Optionally used if INT_TYPE != TIME */
    tform[1] = "1D      ";
    tunit[1] = "";
    ttype[2] = "TSUBINT ";    /* [s] Length of subintegration */
    tform[2] = "1D      ";
    tunit[2] = "";
    ttype[3] = "OFFS_SUB";    /* [s] Offset from Start UTC of subint centre */
    tform[3] = "1D      ";
    tunit[3] = "";
    ttype[4] = "LST_SUB ";    /* [s] LST at subint centre */
    tform[4] = "1D      ";
    tunit[4] = "";
    ttype[5] = "RA_SUB  ";    /* [turns] RA (J2000) at subint centre */
    tform[5] = "1D      ";
    tunit[5] = "";
    ttype[6] = "DEC_SUB ";    /* [turns] Dec (J2000) at subint centre */
    tform[6] = "1D      ";
    tunit[6] = "";
    ttype[7] = "GLON_SUB";    /* [deg] Gal longitude at subint centre */
    tform[7] = "1D      ";
    tunit[7] = "";
    ttype[8] = "GLAT_SUB";    /* [deg] Gal latitude at subint centre */
    tform[8] = "1D      ";
    tunit[8] = "";
    ttype[9] = "FD_ANG  ";    /* [deg] Feed angle at subint centre */
    tform[9] = "1E      ";
    tunit[9] = "";
    ttype[10] = "POS_ANG ";    /* [deg] Position angle of feed at subint centre */
    tform[10] = "1E      ";
    tunit[10] = "";
    ttype[11] = "PAR_ANG ";    /* [deg] Parallactic angle at subint centre */
    tform[11] = "1E      ";
    tunit[11] = "";
    ttype[12] = "TEL_AZ  ";    /* [deg] Telescope azimuth at subint centre */
    tform[12] = "1E      ";
    tunit[12] = "";
    ttype[13] = "TEL_ZEN ";    /* [deg] Telescope zenith angle at subint centre */
    tform[13] = "1E      ";
    tunit[13] = "";

    sprintf( Cstr16, "%dE", num_chans );
    ttype[14] = "DAT_FREQ";
    tform[14] = Cstr16;
    tunit[14] = "";
    ttype[15] = "DAT_WTS ";
    tform[15] = Cstr16;
    tunit[15] = "";

    sprintf( Estr16, "%dE", nscl );
    ttype[16] = "DAT_OFFS";
    tform[16] = Estr16;
    tunit[16] = "";
    ttype[17] = "DAT_SCL ";
    tform[17] = Estr16;
    tunit[17] = "";

    sprintf( Istr16, "%dI", ndata );
    ttype[18] = "DATA    ";
    tform[18] = Istr16;
    tunit[18] = "Jy      ";


    fits_create_tbl( cfitsptr, BINARY_TBL, nrows, ncols, 
		     ttype, tform, tunit, "SUBINT  ", &status);

    /* Add dimensions of column 'ncols' = SUBINT Data */
    naxes[0] = num_bins;
    naxes[1] = num_chans;
    naxes[2] = num_pols;
  
    fits_write_tdim( cfitsptr, ncols, 3, naxes, &status );

    /* Add keywords */
    fits_update_key( cfitsptr, TSTRING, "INT_TYPE", "TIME",
		     "Time axis (TIME, BINPHSPERI, BINLNGASC, etc)", &status);

    fits_update_key( cfitsptr, TSTRING, "INT_UNIT", "SEC",
		     "Unit of time axis (SEC, PHS (0-1), DEG)", &status);

    fits_update_key( cfitsptr, TINT, "NCH_FILE", &num_chans,
		     "Number of channels/sub-bands in this file (I)", &status);
    j = 0;
    fits_update_key( cfitsptr, TINT, "NCH_STRT", &j,
		     "Start channel/sub-band number (0 to NCHAN-1) (I)", &status);
    
    /* Store subint hdu number */
    fits_get_hdu_num( cfitsptr, &subint_hdu );

  }
  
  /* Write SUBINT BINTABLE columns */

  /* Fill in columns of table */
  col = 1;
  
  /* Subint number. If NAXIS=-1, 0 indicates EOD. */
  fits_write_col( cfitsptr, TINT, col, subint_cnt, 1, 1, &subint_cnt, &status );
  col++;
  
  /* INDEXVAL - Optionally used if INT_TYPE != TIME */
  dx = 0.0;
  fits_write_col( cfitsptr, TDOUBLE, col, subint_cnt, 1, 1, &dx, &status );
  col++;

  /* [s] Length of subint */
  fits_write_col( cfitsptr, TDOUBLE, col, subint_cnt, 1, 1, &sum_subint_len_secs, &status );
  col++;

  /* [s] Offset from Start UTC of subint centre */
  dx = sum_subint_mid_pt / sum_subint_len_secs;
  fits_write_col( cfitsptr, TDOUBLE, col, subint_cnt, 1, 1, &dx, &status );
  col++;

  /* [s] LST at subint centre */
  ds = sum_subint_lst_sin / sum_subint_len_secs;
  dc = sum_subint_lst_cos / sum_subint_len_secs;
  if( ( dx = ( atan2( ds, dc ) / TwoPi ) ) < 0.0 ) dx += 1.0;
  dx *= 86400.0;
  fits_write_col( cfitsptr, TDOUBLE, col, subint_cnt, 1, 1, &dx, &status );
  col++;

  /* [turns] RA (J2000) at subint centre */
  ds = sum_subint_ra_sin / sum_subint_len_secs;
  dc = sum_subint_ra_cos / sum_subint_len_secs;
  if( ( dx = ( atan2( ds, dc ) / TwoPi ) ) < 0.0 ) dx += 1.0;
  fits_write_col( cfitsptr, TDOUBLE, col, subint_cnt, 1, 1, &dx, &status );
  col++;

  /* [turns] Dec (J2000) at subint centre */
  ds = sum_subint_dec_sin / sum_subint_len_secs;
  dc = sum_subint_dec_cos / sum_subint_len_secs;
  dx = atan2( ds, dc ) / TwoPi;
  fits_write_col( cfitsptr, TDOUBLE, col, subint_cnt, 1, 1, &dx, &status );
  col++;

  /* [deg] Gal longitude at subint centre */
  ds = sum_subint_Glon_sin / sum_subint_len_secs;
  dc = sum_subint_Glon_cos / sum_subint_len_secs;
  if( ( dx = ( atan2( ds, dc ) / TwoPi ) ) < 0.0 ) dx += 1.0;
  dx *= 360.0;
  fits_write_col( cfitsptr, TDOUBLE, col, subint_cnt, 1, 1, &dx, &status );
  col++;

  /* [deg] Gal latitude at subint centre */
  ds = sum_subint_Glat_sin / sum_subint_len_secs;
  dc = sum_subint_Glat_cos / sum_subint_len_secs;
  dx = atan2( ds, dc ) * 360.0 / TwoPi;
  fits_write_col( cfitsptr, TDOUBLE, col, subint_cnt, 1, 1, &dx, &status );
  col++;

  /* [deg] Feed angle at subint centre */
  ds = sum_subint_fa_sin / sum_subint_len_secs;
  dc = sum_subint_fa_cos / sum_subint_len_secs;
  dx = atan2( ds, dc ) * 360.0 / TwoPi;
  x = (float) dx;
  fits_write_col( cfitsptr, TFLOAT, col, subint_cnt, 1, 1, &x, &status );
  col++;

  /* [deg] Parallactic angle at subint centre */
  ds = sum_subint_pa_sin / sum_subint_len_secs;
  dc = sum_subint_pa_cos / sum_subint_len_secs;
  dy = atan2( ds, dc ) * 360.0 / TwoPi;

  /* [deg] Position angle of feed at subint centre */
  dx = dx + dy;
  if( dx > 180.0 ) dx -= 360.0;
  if( dx < 180.0 ) dx += 360.0;
  x = (float) dx;
  fits_write_col( cfitsptr, TFLOAT, col, subint_cnt, 1, 1, &x, &status );
  col++;

  /* [deg] Parallactic angle at subint centre */
  x = (float) dy;
  fits_write_col( cfitsptr, TFLOAT, col, subint_cnt, 1, 1, &x, &status );
  col++;

  /* [deg] Telescope azimuth at subint centre */
  ds = sum_subint_az_sin / sum_subint_len_secs;
  dc = sum_subint_az_cos / sum_subint_len_secs;
  if( ( dx = ( atan2( ds, dc ) / TwoPi ) ) < 0.0 ) dx += 1.0;
  dx *= 360.0;
  x = (float) dx;
  fits_write_col( cfitsptr, TFLOAT, col, subint_cnt, 1, 1, &x, &status );
  col++;

  /* [deg] Telescope zenith angle at subint centre */
  ds = sum_subint_el_sin / sum_subint_len_secs;
  dc = sum_subint_el_cos / sum_subint_len_secs;
  dx = 90.0 - ( atan2( ds, dc ) * 360.0 / TwoPi );
  x = (float) dx;
  fits_write_col( cfitsptr, TFLOAT, col, subint_cnt, 1, 1, &x, &status );
  col++;


  /* Centre freq. for each channel - NCHAN floats */
  fits_write_col( cfitsptr, TFLOAT, col, subint_cnt, 1, num_chans, binned_freq, &status );
  col++;

  /* Weights for each channel -  NCHAN floats */
  fits_write_col( cfitsptr, TFLOAT, col, subint_cnt, 1, num_chans, binned_weight, &status );
  col++;

  /* Data offset for each channel - NCHAN*NPOL floats */
  fits_write_col( cfitsptr, TFLOAT, col, subint_cnt, 1, nscl, binned_offset, &status );
  col++;

  /* Data scale factor for each channel - NCHAN*NPOL floats */
  fits_write_col( cfitsptr, TFLOAT, col, subint_cnt, 1, nscl, binned_scale, &status );
  col++;

  /* Subint data table - Dimensions of data table = (NBIN,NCHAN,NPOL) */
  fits_write_col( cfitsptr, TSHORT, col, subint_cnt, 1, ndata, binned_data, &status );
  col++;
  
  /* Finished SUBINT */

#define STEP_BACK
#ifdef STEP_BACK

  /* Move to digitiser statistics  */
  fits_movabs_hdu( cfitsptr, dig_stats_hdu, NULL, &status );

  if( subint_cnt == 1 ) {
    
    /* Now we know whether samplers are in fixed or auto mode - Add DIGLEV key */
    if( (*c_c).wb_ifsam.freq[0].ant[0].pol[0].sam_status & WBSAM_FIXED )
      strcpy( astr, "FIX" );
    else 
      strcpy( astr, "AUTO" );
    fits_update_key( cfitsptr, TSTRING, "DIGLEV", astr,
		     "Digitiser level-setting mode (AUTO, FIX)", &status);
  }
  
  /* Add data for this subint */
  fits_write_col( cfitsptr, TFLOAT, 1, subint_cnt, 1, num_wbsam_stats, wbsam_stats, &status );


  /* Move to bandpass  */
  fits_movabs_hdu( cfitsptr, bpass_hdu, NULL, &status );
  /* Overwrite row 1 with latest data  */

  /* Data offset for each receiver channel - NRCVR floats */
  fits_write_col( cfitsptr, TFLOAT, 1, 1, 1, nrcvr, bpass_offset, &status );

  /* Data scale factor for each receiver channel - NRCVR floats */
  fits_write_col( cfitsptr, TFLOAT, 2, 1, 1, nrcvr, bpass_scale, &status );

  /* Bandpass data table - Dimensions of data table = (NCHAN_orig,NRCVR) */
  ndata = nchan_orig * nrcvr;
  fits_write_col( cfitsptr, TSHORT, 3, 1, 1, ndata, bpass_data, &status );


  /* Move back to subint HDU for next subint */
  fits_movabs_hdu( cfitsptr, subint_hdu, NULL, &status );

#endif /* STEP_BACK */

  /* Now FLUSH any internal buffers to the file */
  fits_flush_file( cfitsptr, &status );


  return( status );
  
}



int CDfits_close_reopen_file( void )
{
  return( 0 );
  
}

int CDfits_close_file( void )
{
  int status;
  
  status = 0;
  fits_close_file( cfitsptr, &status );
  return( status );
  
}


