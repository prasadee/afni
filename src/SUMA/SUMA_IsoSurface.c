/*! File to contain routines for creating isosurfaces.

NOTE: MarchingCube code was translated from Thomas Lewiner's C++
implementation of the paper:
Efficient Implementation of Marching Cubes� Cases with Topological Guarantees
by Thomas Lewiner, H�lio Lopes, Ant�nio Wilson Vieira and Geovan Tavares 
in Journal of Graphics Tools. 
http://www-sop.inria.fr/prisme/personnel/Thomas.Lewiner/JGT.pdf
*/

#include "SUMA_suma.h"
#include "MarchingCubes/MarchingCubes.h"

#undef STAND_ALONE

#if defined SUMA_IsoSurface_STANDALONE
#define STAND_ALONE 
#endif

#ifdef STAND_ALONE
/* these global variables must be declared even if they will not be used by this main */
SUMA_SurfaceViewer *SUMAg_cSV = NULL; /*!< Global pointer to current Surface Viewer structure*/
SUMA_SurfaceViewer *SUMAg_SVv = NULL; /*!< Global pointer to the vector containing the various Surface Viewer Structures 
                                    SUMAg_SVv contains SUMA_MAX_SURF_VIEWERS structures */
int SUMAg_N_SVv = 0; /*!< Number of SVs realized by X */
SUMA_DO *SUMAg_DOv = NULL;   /*!< Global pointer to Displayable Object structure vector*/
int SUMAg_N_DOv = 0; /*!< Number of DOs stored in DOv */
SUMA_CommonFields *SUMAg_CF = NULL; /*!< Global pointer to structure containing info common to all viewers */
#else
extern SUMA_CommonFields *SUMAg_CF;
extern SUMA_DO *SUMAg_DOv;
extern SUMA_SurfaceViewer *SUMAg_SVv;
extern int SUMAg_N_SVv; 
extern int SUMAg_N_DOv;  
#endif

/*! a macro version of MarchingCubes' set_data */
#define SUMA_SET_MC_DATA(mcb, val, i, j, k) {  \
   mcb->data[ i + j*mcb->size_x + k*mcb->size_x*mcb->size_y] = val; \
}

/*!
   A function version of the program mc by Thomas Lewiner
   see main.c in ./MarchingCubes
*/
SUMA_SurfaceObject *SUMA_MarchingCubesSurface(SUMA_ISOSURFACE_OPTIONS * Opt)
{
   static char FuncName[]={"SUMA_MarchingCubesSurface"};
   SUMA_SurfaceObject *SO=NULL;
   int nxx, nyy, nzz, cnt, i, j, k, *FaceSetList=NULL;
   float *NodeList=NULL;
   SUMA_NEW_SO_OPT *nsoopt = NULL;
   THD_fvec3 fv, iv;
   MCB *mcp ;
   
   SUMA_ENTRY;
   
   nxx = DSET_NX(Opt->in_vol);
   nyy = DSET_NY(Opt->in_vol);
   nzz = DSET_NZ(Opt->in_vol);
   
   if (Opt->debug) {
      fprintf(SUMA_STDERR,"%s:\nNxx=%d\tNyy=%d\tNzz=%d\n", FuncName, nxx, nyy, nzz);
      fprintf(SUMA_STDERR,"%s:\nChanging mask 0 to -1\n", FuncName);
   }
   for (i=0;i<Opt->nvox; ++i) { if (!Opt->maskv[i]) Opt->maskv[i]=-1; }

   mcp = MarchingCubes(-1, -1, -1);
   set_resolution( mcp, nxx, nyy, nzz ) ;
   
   init_all(mcp) ;
   if (Opt->debug) fprintf(SUMA_STDERR,"%s:\nSetting data...\n", FuncName);
   cnt = 0;
   for(  i = 0 ; i < mcp->size_x ; i++ ) {
      for(  j = 0 ; j < mcp->size_y ; j++ ) {
         for(  k = 0 ; k < mcp->size_z ; k++ ) {
           SUMA_SET_MC_DATA ( mcp, Opt->maskv[cnt], k, j, i); 
            ++cnt;
         }
      }
   }
   
   if (Opt->debug) fprintf(SUMA_STDERR,"%s:\nrunning MarchingCubes...\n", FuncName);
   run(mcp) ;
   clean_temps(mcp) ;
   
   if (Opt->debug > 1) {
      fprintf(SUMA_STDERR,"%s:\nwriting out NodeList and FaceSetList...\n", FuncName);
      write1Dmcb(mcp);
   }
   
   if (Opt->debug) {
      fprintf(SUMA_STDERR,"%s:\nNow creating SO...\n", FuncName);
   }
   
   NodeList = (float *)SUMA_malloc(sizeof(float)*3*mcp->nverts);
   FaceSetList = (int *)SUMA_malloc(sizeof(int)*3*mcp->ntrigs);
   if (!NodeList || !FaceSetList)  {
      SUMA_SL_Crit("Failed to allocate!");
      SUMA_RETURN(SO);
   }
   if (Opt->debug) {
      fprintf(SUMA_STDERR,"%s:\nCopying vertices, changing to DICOM \nOrig:(%f %f %f) \nD:(%f %f %f)...\n", 
         FuncName, DSET_XORG(Opt->in_vol), DSET_YORG(Opt->in_vol), DSET_ZORG(Opt->in_vol),
                     DSET_DX(Opt->in_vol), DSET_DY(Opt->in_vol), DSET_DZ(Opt->in_vol));
   }
   for ( i = 0; i < mcp->nverts; i++ ) {
      j = 3*i; /* change from index coordinates to mm DICOM, next three lines are equivalent of SUMA_THD_3dfind_to_3dmm*/
      fv.xyz[0] = DSET_XORG(Opt->in_vol) + mcp->vertices[i].x * DSET_DX(Opt->in_vol);
      fv.xyz[1] = DSET_YORG(Opt->in_vol) + mcp->vertices[i].y * DSET_DY(Opt->in_vol);
      fv.xyz[2] = DSET_ZORG(Opt->in_vol) + mcp->vertices[i].z * DSET_DZ(Opt->in_vol);
      /* change mm to RAI coords */
		iv = SUMA_THD_3dmm_to_dicomm( Opt->in_vol->daxes->xxorient, Opt->in_vol->daxes->yyorient, Opt->in_vol->daxes->zzorient, fv );
      NodeList[j  ] = iv.xyz[0];
      NodeList[j+1] = iv.xyz[1];
      NodeList[j+2] = iv.xyz[2];
   }
   for ( i = 0; i < mcp->ntrigs; i++ ) {
      j = 3*i;
      FaceSetList[j  ] = mcp->triangles[i].v1;
      FaceSetList[j+1] = mcp->triangles[i].v2;
      FaceSetList[j+2] = mcp->triangles[i].v3;
   }
   
   nsoopt = SUMA_NewNewSOOpt();
   SO = SUMA_NewSO(&NodeList, mcp->nverts, &FaceSetList, mcp->ntrigs, nsoopt);

   if (Opt->debug) {
      fprintf(SUMA_STDERR,"%s:\nCleaning mcp...\n", FuncName);
   }
   clean_all(mcp) ;
   free(mcp);
   
   SUMA_RETURN(SO);
}
/*!
   Shamelessly stolen from Rick who stole it from Bob. To hide this theft, I use longer names for functions and variables.
*/
SUMA_Boolean SUMA_Get_isosurface_datasets (SUMA_ISOSURFACE_OPTIONS * Opt)
{
   static char FuncName[]={"SUMA_validate_isosurface_datasets"};
   int i;
   
   SUMA_ENTRY;
   
   Opt->in_vol = THD_open_dataset( Opt->in_name );
   if (!ISVALID_DSET(Opt->in_vol)) {
      if (!Opt->in_name) {
         SUMA_SL_Err("NULL input volume.");
         SUMA_RETURN(NOPE);
      } else {
         SUMA_SL_Err("invalid volume.");
         SUMA_RETURN(NOPE);
      }
   } else if ( DSET_BRICK_TYPE(Opt->in_vol, 0) == MRI_complex) {
      SUMA_SL_Err("Can't do complex data.");
      SUMA_RETURN(NOPE);
   }
   
   Opt->nvox = DSET_NVOX( Opt->in_vol );
   if (DSET_NVALS( Opt->in_vol) != 1) {
      SUMA_SL_Err("Input volume can only have one sub-brick in it.\nUse [.] selectors to choose sub-brick needed.");
      SUMA_RETURN(NOPE);
   }
   
   
   Opt->maskv = (int *)SUMA_malloc(sizeof(int)*Opt->nvox);
   if (!Opt->maskv) {
      SUMA_SL_Crit("Failed to allocate for maskv");
      SUMA_RETURN(NOPE);
   }
   
   switch (Opt->MaskMode) {
      case SUMA_ISO_CMASK:
         if (Opt->cmask) {
            /* here's the second order grand theft */
            int    clen = strlen( Opt->cmask );
	         char * cmd;
            byte *bmask;

	         /* save original cmask command, as EDT_calcmask() is destructive */
	         cmd = (char *)malloc((clen + 1) * sizeof(char));
	         strcpy( cmd,  Opt->cmask);

	         bmask = EDT_calcmask( cmd, &Opt->ninmask );

	         free( cmd );			   /* free EDT_calcmask() string */

	         if ( bmask == NULL ) {
	            SUMA_SL_Err("Failed to compute mask from -cmask option");
	            SUMA_free(Opt->maskv); Opt->maskv=NULL;
               SUMA_RETURN(NOPE);
	         } 
	         if ( Opt->ninmask != Opt->nvox ) {
	            SUMA_SL_Err("Input and cmask datasets do not have "
		                     "the same dimensions\n" );
	            SUMA_free(Opt->maskv); Opt->maskv=NULL;
	            SUMA_RETURN(NOPE);
	         }
	         Opt->ninmask = THD_countmask( Opt->ninmask, bmask );
            for (i=0; i<Opt->nvox; ++i) Opt->maskv[i] = (int)bmask[i];
            free(bmask);bmask=NULL;
         } else {
            SUMA_SL_Err("NULL cmask"); SUMA_RETURN(NOPE);
         }
         break;
      case SUMA_ISO_VAL:
      case SUMA_ISO_RANGE:
         /* load the dset */
         DSET_load(Opt->in_vol);
         Opt->dvec = (double *)SUMA_malloc(sizeof(double) * Opt->nvox);
         if (!Opt->dvec) {
            SUMA_SL_Crit("Faile to allocate for dvec.\nOh misery.");
            SUMA_RETURN(NOPE);
         }
         EDIT_coerce_scale_type( Opt->nvox , DSET_BRICK_FACTOR(Opt->in_vol,0) ,
                                 DSET_BRICK_TYPE(Opt->in_vol,0), DSET_ARRAY(Opt->in_vol, 0) ,      /* input  */
                                 MRI_double               , Opt->dvec  ) ;   /* output */
         /* no need for data in input volume anymore */
         PURGE_DSET(Opt->in_vol);
         
         Opt->ninmask = 0;
         if (Opt->MaskMode == SUMA_ISO_VAL) {
            for (i=0; i<Opt->nvox; ++i) {
               if (Opt->dvec[i] == Opt->v0) { 
                  Opt->maskv[i] = 1; ++ Opt->ninmask; 
               }  else Opt->maskv[i] = 0;
            }
         } else if (Opt->MaskMode == SUMA_ISO_RANGE) {
            for (i=0; i<Opt->nvox; ++i) {
               if (Opt->dvec[i] >= Opt->v0 && Opt->dvec[i] < Opt->v1) { 
                  Opt->maskv[i] = 1; ++ Opt->ninmask; 
               }  else Opt->maskv[i] = 0;
            }
         } else {
            SUMA_SL_Err("Bad Miracle.");
            SUMA_RETURN(NOPE);
         }
         SUMA_free(Opt->dvec); Opt->dvec = NULL; /* this vector is not even created in SUMA_ISO_CMASK mode ...*/
         break;
      default:
         SUMA_SL_Err("Unexpected value of MaskMode");
         SUMA_RETURN(NOPE);
         break;   
   }
   
   if ( Opt->ninmask  <= 0 ) {
	   SUMA_SL_Err("No isovolume found!\nNothing to do." );
	   SUMA_RETURN(NOPE);
	}
   
   if (Opt->debug > 0) {
      fprintf( SUMA_STDERR, "%s:\nInput dset %s has nvox = %d, nvals = %d",
		                        FuncName, Opt->in_name, Opt->nvox, DSET_NVALS(Opt->in_vol) );
	   if ( Opt->maskv == NULL )
	      fprintf( SUMA_STDERR, "\n");
	   else
	      fprintf( SUMA_STDERR, " (%d voxels in mask)\n", Opt->ninmask );
   }
   SUMA_RETURN(YUP);
}

#ifdef SUMA_IsoSurface_STANDALONE

void usage_SUMA_IsoSurface ()
   {
      static char FuncName[]={"usage_SUMA_IsoSurface"};
      char * s = NULL;
      s = SUMA_help_basics();
      printf ( "\n"
               "Usage: A program to perform isosurface extraction from a volume.\n"
               "  IsoSurface  < -input VOL >\n"
               "              < -isoval V | -isorange V0 V1 | -isocmask MASK_COM >\n"
               "              [< -o_TYPE PREFIX>]\n"
               "              [< -debug DBG >]\n"  
               "\n"
               "  Mandatory parameters:\n"
               "     -input VOL: Input volume.\n"
               "     You must use of the iso* options:\n"
               "     -isoval V: Create isosurface where volume = V\n"
               "     -isorange V0 V1: Create isosurface where V0 <= volume < V1\n"
               "     -isocmask MASK_COM: Create isosurface where MASK_COM != 0\n"
               "        For example: -isocmask '-a VOL+orig -expr (1-bool(a-V))' \n"
               "        is equivalent to using -isoval V. \n"
               "\n"
               "  Optional Parameters:\n"
               "     -o_TYPE PREFIX: prefix of output surface.\n"
               "        where TYPE specifies the format of the surface\n"
               "        and PREFIX is, well, the prefix.\n"
               "        TYPE is one of: fs, 1d (or vec), sf, ply.\n"
               "        Default is: -o_ply \n"
               "\n"
               "     -debug DBG: debug levels of 0 (default), 1, 2, 3.\n"
               "        This is no Rick Reynolds debug, which is oft nicer\n"
               "        than the results, but it will do.\n"
               "\n" 
               "%s"
               "\n", s);
       SUMA_free(s); s = NULL;        
       s = SUMA_New_Additions(0, 1); printf("%s\n", s);SUMA_free(s); s = NULL;
       printf("       Ziad S. Saad SSCC/NIMH/NIH ziad@nih.gov     \n");
       exit (0);
   }
/*!
   \brief parse the arguments for SurfSmooth program
   
   \param argv (char *)
   \param argc (int)
   \return Opt (SUMA_ISOSURFACE_OPTIONS *) options structure.
               To free it, use 
               SUMA_free(Opt->out_name); 
               SUMA_free(Opt);
*/
SUMA_ISOSURFACE_OPTIONS *SUMA_IsoSurface_ParseInput (char *argv[], int argc)
{
   static char FuncName[]={"SUMA_IsoSurface_ParseInput"}; 
   SUMA_ISOSURFACE_OPTIONS *Opt=NULL;
   int kar, i, ind;
   char *outname;
   SUMA_Boolean brk = NOPE;
   SUMA_Boolean LocalHead = NOPE;

   SUMA_ENTRY;
   
   Opt = (SUMA_ISOSURFACE_OPTIONS *)SUMA_malloc(sizeof(SUMA_ISOSURFACE_OPTIONS));
   
   kar = 1;
   Opt->spec_file = NULL;
   Opt->out_prefix = NULL;
   Opt->sv_name = NULL;
   Opt->N_surf = -1;
   Opt->in_name = NULL;
   Opt->cmask = NULL;
   Opt->MaskMode = SUMA_ISO_UNDEFINED;
   for (i=0; i<ISOSURFACE_MAX_SURF; ++i) { Opt->surf_names[i] = NULL; }
   outname = NULL;
   Opt->in_vol = NULL;
   Opt->nvox = -1;
   Opt->ninmask = -1;
   Opt->maskv = NULL;
   Opt->debug = 0;
   Opt->v0 = -1.0;
   Opt->v1 = -1.0;
   Opt->dvec = NULL;
   Opt->SurfFileType = SUMA_PLY;
   Opt->SurfFileFormat = SUMA_ASCII;
	brk = NOPE;
	while (kar < argc) { /* loop accross command ine options */
		/*fprintf(stdout, "%s verbose: Parsing command line...\n", FuncName);*/
		if (strcmp(argv[kar], "-h") == 0 || strcmp(argv[kar], "-help") == 0) {
			 usage_SUMA_IsoSurface();
          exit (0);
		}
		
		SUMA_SKIP_COMMON_OPTIONS(brk, kar);
      
      #if 0 /* no support yet */
      if (!brk && (strcmp(argv[kar], "-o_bv") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -o_bv \n");
				exit (1);
			}
         Opt->out_prefix = SUMA_copy_string(argv[kar]);
         Opt->SurfFileType = SUMA_BRAIN_VOYAGER;
         Opt->SurfFileFormat = SUMA_BINARY;
			brk = YUP;
		}
      #endif
      if (!brk && (strcmp(argv[kar], "-o_fs") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -o_fs \n");
				exit (1);
			}
         Opt->out_prefix = SUMA_copy_string(argv[kar]);
         Opt->SurfFileType = SUMA_FREE_SURFER;
         Opt->SurfFileFormat = SUMA_ASCII;
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-o_sf") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -o_sf \n");
				exit (1);
			}
         Opt->out_prefix = SUMA_copy_string(argv[kar]);
         Opt->SurfFileType = SUMA_SUREFIT;
         Opt->SurfFileFormat = SUMA_ASCII;
			brk = YUP;
		}
      
      if (!brk && ( (strcmp(argv[kar], "-o_vec") == 0) || (strcmp(argv[kar], "-o_1d") == 0) ) ) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -o_1d or -o_vec \n");
				exit (1);
			}
         Opt->out_prefix = SUMA_copy_string(argv[kar]);
         Opt->SurfFileType = SUMA_VEC;
         Opt->SurfFileFormat = SUMA_ASCII;
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-o_ply") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -o_ply \n");
				exit (1);
			}
         Opt->out_prefix = SUMA_copy_string(argv[kar]);
         Opt->SurfFileType = SUMA_PLY;
         Opt->SurfFileFormat = SUMA_FF_NOT_SPECIFIED;
			brk = YUP;
		}
      
    
      if (!brk && (strcmp(argv[kar], "-debug") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -debug \n");
				exit (1);
			}
			Opt->debug = atoi(argv[kar]);
         if (Opt->debug > 2) { LocalHead = YUP; }
         brk = YUP;
		}
            
      if (!brk && (strcmp(argv[kar], "-isocmask") == 0)) {
         if (Opt->MaskMode != SUMA_ISO_UNDEFINED) {
            fprintf (SUMA_STDERR, "only one masking mode (-iso*) allowed.\n");
         }
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -isocmask \n");
				exit (1);
			}
			Opt->cmask = argv[kar];
         Opt->MaskMode = SUMA_ISO_CMASK;
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-isoval") == 0)) {
         if (Opt->MaskMode != SUMA_ISO_UNDEFINED) {
            fprintf (SUMA_STDERR, "only one masking mode (-iso*) allowed.\n");
         }
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -isoval \n");
				exit (1);
			}
			Opt->v0 = atof(argv[kar]);
         Opt->MaskMode = SUMA_ISO_VAL;
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-isorange") == 0)) {
         if (Opt->MaskMode != SUMA_ISO_UNDEFINED) {
            fprintf (SUMA_STDERR, "only one masking mode (-iso*) allowed.\n");
         }
         kar ++;
			if (kar+1 >= argc)  {
		  		fprintf (SUMA_STDERR, "need 2 arguments after -isorange \n");
				exit (1);
			}
			Opt->v0 = atof(argv[kar]);kar ++;
         Opt->v1 = atof(argv[kar]);
         Opt->MaskMode = SUMA_ISO_RANGE;
         if (Opt->v1 < Opt->v0) {
            fprintf (SUMA_STDERR, "range values wrong. Must have %f <= %f \n", Opt->v0, Opt->v1);
				exit (1);
         }
			brk = YUP;
		}
      if (!brk && (strcmp(argv[kar], "-input") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -input \n");
				exit (1);
			}
			Opt->in_name = SUMA_copy_string(argv[kar]);
         brk = YUP;
		}
      
      if (!brk) {
			fprintf (SUMA_STDERR,"Error %s:\nOption %s not understood. Try -help for usage\n", FuncName, argv[kar]);
			exit (1);
		} else {	
			brk = NOPE;
			kar ++;
		}
   }
   
   if (!Opt->out_prefix) Opt->out_prefix = SUMA_copy_string("isosurface_out");
   
   SUMA_RETURN(Opt);
}

int main (int argc,char *argv[])
{/* Main */    
   static char FuncName[]={"IsoSurface"}; 
	int i;
   void *SO_name=NULL;
   SUMA_SurfaceObject *SO = NULL;
   SUMA_ISOSURFACE_OPTIONS *Opt;  
   char  stmp[200];
   SUMA_Boolean exists = NOPE;
   SUMA_Boolean LocalHead = NOPE;
   
	SUMA_mainENTRY;
   SUMA_STANDALONE_INIT;
   
   
   /* Allocate space for DO structure */
	SUMAg_DOv = SUMA_Alloc_DisplayObject_Struct (SUMA_MAX_DISPLAYABLE_OBJECTS);
   
   if (argc < 2) {
      usage_SUMA_IsoSurface();
      exit (1);
   }
   
   Opt = SUMA_IsoSurface_ParseInput (argv, argc);
   
   SO_name = SUMA_Prefix2SurfaceName(Opt->out_prefix, NULL, NULL, Opt->SurfFileType, &exists);
   if (exists) {
      fprintf(SUMA_STDERR,"Error %s:\nOutput file(s) %s* on disk.\nWill not overwrite.\n", FuncName, Opt->out_prefix);
      exit(1);
   }
   
   if (Opt->debug) {
      SUMA_S_Note("Creating mask...");
   }
   if (!SUMA_Get_isosurface_datasets (Opt)) {
      SUMA_SL_Err("Failed to get data.");
      exit(1);
   }
   
   if (Opt->debug > 1) {
      if (Opt->debug == 2) {
         FILE *fout=fopen("inmaskvec.1D","w");
         SUMA_S_Note("Writing masked values...\n");
         if (!fout) {
            SUMA_SL_Err("Failed to write maskvec");
            exit(1);
         }
         fprintf(fout,  "#Col. 0 Voxel Index\n"
                        "#Col. 1 Is a mask (all values here should be 1)\n" );
         for (i=0; i<Opt->nvox; ++i) {
            if (Opt->maskv[i]) {
               fprintf(fout,"%d %d\n", i, Opt->maskv[i]);
            }
         }
         fclose(fout); fout = NULL;
      } else {
         FILE *fout=fopen("maskvec.1D","w");
         SUMA_S_Note("Writing all mask values...\n");
         if (!fout) {
            SUMA_S_Err("Failed to write maskvec");
            exit(1);
         }
         fprintf(fout,  "#Col. 0 Voxel Index\n"
                        "#Col. 1 Is in mask ?\n" );
         for (i=0; i<Opt->nvox; ++i) {
            fprintf(fout,"%d %d\n", i, Opt->maskv[i]);
         }
         fclose(fout); fout = NULL;
      }
   }
   
   /* Now call Marching Cube functions */
   if (!(SO = SUMA_MarchingCubesSurface(Opt))) {
      SUMA_S_Err("Failed to create surface.\n");
      exit(1);
   }

   /* write the surface to disk */
   if (!SUMA_Save_Surface_Object (SO_name, SO, Opt->SurfFileType, Opt->SurfFileFormat, NULL)) {
      fprintf (SUMA_STDERR,"Error %s: Failed to write surface object.\n", FuncName);
      exit (1);
   }
   
   if (Opt->dvec) SUMA_free(Opt->dvec); Opt->dvec = NULL;
   if (Opt->maskv) {SUMA_free(Opt->maskv); Opt->maskv = NULL;} 
   if (Opt->in_vol) { DSET_delete( Opt->in_vol); Opt->in_vol = NULL;} 
   if (Opt->out_prefix) SUMA_free(Opt->out_prefix); Opt->out_prefix = NULL;
   if (Opt) SUMA_free(Opt);
   if (!SUMA_Free_Displayable_Object_Vect (SUMAg_DOv, SUMAg_N_DOv)) {
      SUMA_SL_Err("DO Cleanup Failed!");
   }
   if (SO_name) SUMA_free(SO_name); SO_name = NULL;
   if (!SUMA_Free_CommonFields(SUMAg_CF)) SUMA_error_message(FuncName,"SUMAg_CF Cleanup Failed!",1);
   exit(0);
}
#endif      
