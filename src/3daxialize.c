#include "mrilib.h"
#define MAIN
#include "dbtrace.h"

/*****************************************************************************
  This software is copyrighted and owned by the Medical College of Wisconsin.
  See the file README.Copyright for details.
******************************************************************************/

/*--- This program will read in a dataset and write it out in axial order ---*/

int main( int argc , char * argv[] )
{
   THD_3dim_dataset * old_dset , * new_dset ;
   FD_brick ** fbr , * brax ;
   int iarg , a1,a2,a3 , aa1,aa2,aa3 , ival,kk,cmode,npix,dsiz,code ;
   THD_ivec3 iv_nxyz   , iv_xyzorient ;
   THD_fvec3 fv_xyzorg , fv_xyzdel ;
   float xyz_org[4] , xyz_del[4] , brfac_save ;
   MRI_IMAGE * im ;
   void * imar ;
   FILE * far ;
   THD_datablock * old_dblk , * new_dblk ;
   char new_prefix[THD_MAX_PREFIX] = "axialize" ;
   int verbose = 0 , nim , pim ;

   /*- sanity check -*/

   if( argc < 2 || strcmp(argv[1],"-help") == 0 ){
      printf("Usage: 3daxialize [options] dataset\n"
             "Purpose: Read in a dataset and write it out as a new dataset\n"
             "         with the data brick oriented as axial slices.\n"
             "         The input dataset must have a .BRIK file.\n"
             "         One application is to create a dataset that can\n"
             "         be used with the AFNI volume rendering plugin.\n"
             "\n"
             "Options:\n"
             " -prefix ppp = Use 'ppp' as the prefix for the new dataset.\n"
             "               [default = 'axialize']\n"
             " -verbose    = Print out a progress pacifier.\n"
            ) ;
      exit(0) ;
   }

   /*- options -*/

   iarg = 1 ;
   while( argv[iarg][0] == '-' ){

      if( strcmp(argv[iarg],"-prefix") == 0 ){
         strcpy( new_prefix , argv[++iarg] ) ;
         if( !THD_filename_ok(new_prefix) ){
            fprintf(stderr,"** illegal new prefix: %s\n",new_prefix); exit(1);
         }
         iarg++ ; continue ;
      }

      if( strcmp(argv[iarg],"-verbose") == 0 ){
         verbose++ ; iarg++ ; continue ;
      }

      fprintf(stderr,"** Unknown option: %s\n",argv[iarg]); exit(1);
   }

   /*- get input dataset -*/

   old_dset = THD_open_one_dataset( argv[iarg] ) ;
   if( old_dset == NULL ){
      fprintf(stderr,"** can't open input dataset: %s\n",argv[iarg]) ; exit(1) ;
   }

   if( verbose ) printf("++ Loading input dataset %s\n",argv[iarg]) ;

   DSET_load(old_dset) ;
   if( !DSET_LOADED(old_dset) ){
      fprintf(stderr,"** can't load input dataset .BRIK: %s\n",argv[iarg]) ; exit(1) ;
   }

   /*- setup output dataset -*/

   fbr = THD_setup_bricks( old_dset ) ; brax = fbr[0] ;

   new_dset = EDIT_empty_copy( old_dset ) ;

   LOAD_IVEC3( iv_nxyz      , brax->n1    , brax->n2    , brax->n3     ) ;
   LOAD_IVEC3( iv_xyzorient , ORI_R2L_TYPE, ORI_A2P_TYPE, ORI_I2S_TYPE ) ;

   LOAD_FVEC3( fv_xyzdel    , brax->del1  , brax->del2  , brax->del3   ) ;

   UNLOAD_IVEC3( brax->a123 , a1,a2,a3 ) ;
   aa1 = abs(a1) ; aa2 = abs(a2) ; aa3 = abs(a3) ;
   xyz_org[1] = new_dset->daxes->xxorg ; xyz_del[1] = new_dset->daxes->xxdel ;
   xyz_org[2] = new_dset->daxes->yyorg ; xyz_del[2] = new_dset->daxes->yydel ;
   xyz_org[3] = new_dset->daxes->zzorg ; xyz_del[3] = new_dset->daxes->zzdel ;
   LOAD_FVEC3( fv_xyzorg ,
               (a1 > 0) ? xyz_org[aa1] : xyz_org[aa1]+(brax->n1-1)*xyz_del[aa1],
               (a2 > 0) ? xyz_org[aa2] : xyz_org[aa2]+(brax->n2-1)*xyz_del[aa2],
               (a3 > 0) ? xyz_org[aa3] : xyz_org[aa3]+(brax->n3-1)*xyz_del[aa3] ) ;

   EDIT_dset_items( new_dset ,
                       ADN_nxyz      , iv_nxyz ,
                       ADN_xyzdel    , fv_xyzdel ,
                       ADN_xyzorg    , fv_xyzorg ,
                       ADN_xyzorient , iv_xyzorient ,
                       ADN_prefix    , new_prefix ,
                    ADN_none ) ;

   /*- prepare to write new dataset -*/

   new_dblk = new_dset->dblk ;
   old_dblk = old_dset->dblk ;

   cmode = THD_get_write_compression() ;
   far   = COMPRESS_fopen_write( new_dblk->diskptr->brick_name, cmode ) ;
   npix  = brax->n1 * brax->n2 ;

   /*- get slices from input, write to disk -*/

   if( verbose ){
      printf("++ Writing new dataset .BRIK"); fflush(stdout);
      pim = brax->n3 / 5 ; if( pim < 1 ) pim = 1 ;
   }

   for( nim=ival=0 ; ival < DSET_NVALS(old_dset) ; ival++ ){
      brfac_save = DBLK_BRICK_FACTOR(new_dblk,ival) ;
      DBLK_BRICK_FACTOR(new_dblk,ival) = 0.0 ;
      DBLK_BRICK_FACTOR(old_dblk,ival) = 0.0 ;
      dsiz = mri_datum_size( DSET_BRICK_TYPE(new_dset,ival) ) ;
      for( kk=0 ; kk < brax->n3 ; kk++ ){
         im   = FD_warp_to_mri( kk , ival , brax ) ;
         imar = mri_data_pointer(im) ;
         code = fwrite( imar , dsiz , npix , far ) ;
         mri_free(im) ;

         if( verbose && (++nim)%pim == 1 ){ printf("."); fflush(stdout); }
      }
      DBLK_BRICK_FACTOR(new_dblk,ival) = brfac_save ;
      DSET_unload_one(old_dset,ival) ;

      if( verbose ){ printf("!"); fflush(stdout); }
   }
   COMPRESS_fclose(far) ;
   if( verbose ){ printf("\n") ; fflush(stdout); }

   DSET_unload( old_dset ) ;

   /*- do the output header -*/

   if( verbose ) printf("++ Writing new dataset .HEAD\n") ;

   DSET_load( new_dset ) ; THD_load_statistics( new_dset ) ;
   THD_write_3dim_dataset( NULL,NULL , new_dset , False ) ;
   exit(0) ;
}
