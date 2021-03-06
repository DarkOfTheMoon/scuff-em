/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of SCUFF-EM.
 *
 * SCUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * SCUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * OutputModules.cc -- various types of 'output modules' for scuff-spectrum
 *
 */
#include <stdio.h>
#include <math.h>
#include <stdarg.h>

#include <libscuff.h>

#define MAXSTR   1000 

#define ABSTOL   0.0
#define RELTOL   5.0e-2
#define MAXEVALS 20000

#define II cdouble(0.0,1.0)

using namespace scuff;

// prototype for non-class method in libscuff for fetching
// spherical moments
HVector *GetSphericalMoments(RWGGeometry *G, cdouble Omega, int lMax,
                             HVector *KN, HVector *MomentVector=0);

/***************************************************************/
/* postprocessing of eigenvector data, part 1: write eigenmode */
/* field components at user-specified list of evaluation points*/
/***************************************************************/
void ProcessEPFile(RWGGeometry *G, HVector *KN, cdouble Omega, double *kBloch,
                   char *EPFileName, char *OutFileBase)
{ 
  /*--------------------------------------------------------------*/
  /*- try to read eval points from file --------------------------*/
  /*--------------------------------------------------------------*/
  HMatrix *XMatrix=new HMatrix(EPFileName,LHM_TEXT,"-ncol 3");
  if (XMatrix->ErrMsg)
   { fprintf(stderr,"Error processing EP file: %s\n",XMatrix->ErrMsg);
     delete XMatrix;
     return;
   };
  Log("Evaluating fields at points in file %s...",EPFileName);
  HMatrix *FMatrix = G->GetFields( 0, KN, Omega, kBloch, XMatrix);

  /*--------------------------------------------------------------*/
  /*- get components of fields radiated by eigenmode currents    -*/
  /*--------------------------------------------------------------*/
  SetDefaultCD2SFormat("%+.8e %+.8e ");
  FILE *f=vfopen("%s.%s.ModeFields","w",OutFileBase,GetFileBase(EPFileName));
  fprintf(f,"# scuff-spectrum run on %s (%s)\n",GetHostName(),GetTimeString());
  fprintf(f,"# columns: \n");
  fprintf(f,"# 1,2,3   x,y,z (evaluation point coordinates)\n");
  fprintf(f,"# 4 5     re,im omega (angular frequency)\n");
  int nc=6;
  if (G->LDim==1)
   fprintf(f,"# %i      kx (bloch wavevector)\n",nc++);
  if (G->LDim==2)
   fprintf(f,"# %i      ky (bloch wavevector)\n",nc++);
  fprintf(f,"# %02i,%02i   real, imag Ex\n",nc,nc+1); nc+=2;
  fprintf(f,"# %02i,%02i   real, imag Ey\n",nc,nc+1); nc+=2;
  fprintf(f,"# %02i,%02i   real, imag Ez\n",nc,nc+1); nc+=2;
  fprintf(f,"# %02i,%02i   real, imag Hx\n",nc,nc+1); nc+=2;
  fprintf(f,"# %02i,%02i   real, imag Hy\n",nc,nc+1); nc+=2;
  fprintf(f,"# %02i,%02i   real, imag Hz\n",nc,nc+1); nc+=2;
  for(int nr=0; nr<FMatrix->NR; nr++)
   { double X[3];
     cdouble EH[6];
     XMatrix->GetEntriesD(nr,":",X);
     FMatrix->GetEntries(nr,":",EH);
     fprintVec(f,X,3);
     fprintf(f,"%s ",z2s(Omega));
     fprintVecCR(f,EH,6);
   };
  fclose(f);

  delete XMatrix;
  delete FMatrix;

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
static const char *FieldTitles[]=
 {"Ex", "Ey", "Ez", "|E|",
  "Hx", "Hy", "Hz", "|H|",
 };

#define NUMFIELDFUNCS 8

void WriteFVMesh(RWGGeometry *G, HVector *KN, cdouble Omega, double *kBloch,
                 RWGSurface *FluxMesh, FILE *f)
{
  RWGPanel **Panels = FluxMesh->Panels;
  int NumPanels     = FluxMesh->NumPanels;
  double *Vertices  = FluxMesh->Vertices;
  int NumVertices   = FluxMesh->NumVertices;

  /*--------------------------------------------------------------*/
  /*- create an Nx3 HMatrix whose columns are the coordinates of  */
  /*- the flux mesh panel vertices                                */
  /*--------------------------------------------------------------*/
  HMatrix *XMatrix=new HMatrix(NumVertices, 3);
  for(int nv=0; nv<NumVertices; nv++)
   XMatrix->SetEntriesD(nv, ":", Vertices + 3*nv);

  /*--------------------------------------------------------------*/
  /* 20150404 explain me -----------------------------------------*/
  /*--------------------------------------------------------------*/
  int nvRef=Panels[0]->VI[0];
  for(int nv=0; nv<NumVertices; nv++)
   { 
     bool VertexUsed=false;
     for(int np=0; np<NumPanels && !VertexUsed; np++)
      if (     nv==Panels[np]->VI[0]
           ||  nv==Panels[np]->VI[1]
           ||  nv==Panels[np]->VI[2]
         ) VertexUsed=true;

     if (!VertexUsed)
      { printf("Replacing %i:{%.2e,%.2e,%.2e} with %i: {%.2e,%.2e,%.2e}\n",
                nv,XMatrix->GetEntryD(nv,0), XMatrix->GetEntryD(nv,1), XMatrix->GetEntryD(nv,2),
                nvRef,Vertices[3*nvRef+0], Vertices[3*nvRef+1], Vertices[3*nvRef+2]);
        XMatrix->SetEntriesD(nv, ":", Vertices + 3*nvRef);
      };
   };

  /*--------------------------------------------------------------*/
  /*- get the total fields at the panel vertices                 -*/
  /*--------------------------------------------------------------*/
  HMatrix *FMatrix=G->GetFields(0, KN, Omega, kBloch, XMatrix);
  for(int nff=0; nff<NUMFIELDFUNCS; nff++)
   { 
     //if (FuncString && !strcasestr(FuncString,FieldTitles[nff]))
     // continue;
     fprintf(f,"View \"%s (Omega%s)",FieldTitles[nff],z2s(Omega));
     fprintf(f,"\" {\n");

     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     for(int np=0; np<NumPanels; np++)
      { 
        RWGPanel *P=Panels[np];

        double *V[3]; // vertices
        double Q[3];  // quantities
        for(int nv=0; nv<3; nv++)
         { 
           int VI = P->VI[nv];
           V[nv]  = Vertices + 3*VI;

           cdouble EH[6];
           FMatrix->GetEntries(VI, ":", EH);
           cdouble *F = (nff>=4) ? EH+0 : EH+3;
           if (nff==3 || nff==7)
            Q[nv] = sqrt(norm(F[0]) + norm(F[1]) + norm(F[2]));
           else
            Q[nv] = abs( F[nff%4] );
         };
           
        fprintf(f,"ST(%e,%e,%e,%e,%e,%e,%e,%e,%e) {%e,%e,%e};\n",
                   V[0][0], V[0][1], V[0][2],
                   V[1][0], V[1][1], V[1][2],
                   V[2][0], V[2][1], V[2][2],
                   Q[0], Q[1], Q[2]);
      }; // for(int np=0; np<FluxMesh->NumPanels; np++)

     fprintf(f,"};\n\n");

   };  // for(int nff=0; nff<NUMFIELDFUNCS; nff++)

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  delete FMatrix;
  delete XMatrix;
}

/***************************************************************/
/* VisualizeFields() produces a color plot of the E and H      */
/* fields on a user-specified surface mesh for visualization   */
/* in GMSH.                                                    */
/***************************************************************/
void VisualizeFields(RWGGeometry *G, HVector *KN, cdouble Omega, double *kBloch,
                     char *OutFileBase, char *FVMesh, char *TransFile)
{ 
  int NumFVMeshTransforms;
  GTComplex **FVMeshGTCList=ReadTransFile(TransFile, &NumFVMeshTransforms);
  
  /*--------------------------------------------------------------*/
  /*- try to open user's mesh file -------------------------------*/
  /*--------------------------------------------------------------*/
  RWGSurface *FluxMesh=new RWGSurface(FVMesh);

  char *GeoFileBase=OutFileBase;
  char *FVMFileBase=GetFileBase(FVMesh);
  for(int nt=0; nt<NumFVMeshTransforms; nt++)
   {
     char *Tag = FVMeshGTCList[nt]->Tag;
     char PPFileName[100];
     if (NumFVMeshTransforms>1)
      { 
        snprintf(PPFileName,100,"%s.%s.%s.pp",GeoFileBase,FVMFileBase,Tag);
        Log("Creating flux plot for surface %s, transform %s...",FVMesh,Tag);
      }
     else
      {  snprintf(PPFileName,100,"%s.%s.pp",GeoFileBase,FVMFileBase);
         Log("Creating flux plot for surface %s...",FVMesh);
      };
     FILE *f=fopen(PPFileName,"a");
     if (!f) 
      { Warn("could not open field visualization file %s",PPFileName);
       continue;
      };

     FluxMesh->Transform(FVMeshGTCList[nt]->GT);
     WriteFVMesh(G, KN, Omega, kBloch, FluxMesh, f);
     FluxMesh->UnTransform();

     fclose(f);
   };

  delete FluxMesh;
  DestroyGTCList(FVMeshGTCList,NumFVMeshTransforms);

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteCartesianMoments(RWGGeometry *G, HVector *KN,
                           cdouble Omega, double *kBloch,
                           char *CartesianMomentFile)
{
  /***************************************************************/
  /* write file preamble on initial file creation ****************/
  /***************************************************************/
  FILE *f=fopen(CartesianMomentFile,"r");
  if (!f)
   { f=fopen(CartesianMomentFile,"w");
     if ( !f ) 
      { Warn ("could not open file %s (skipping Cartesian moments...)",CartesianMomentFile);
        return;
      };
     fprintf(f,"# data file columns: \n");
     fprintf(f,"# 1     angular frequency (3e14 rad/sec)\n");
     int nc=2;
     fprintf(f,"# %i     surface label\n",nc++);
     fprintf(f,"# %02i,%02i real,imag px (electric dipole moment)\n",nc,nc+1); nc+=2;
     fprintf(f,"# %02i,%02i real,imag py \n",nc,nc+1); nc+=2;
     fprintf(f,"# %02i,%02i real,imag pz \n",nc,nc+1); nc+=2;
     fprintf(f,"# %02i,%02i real,imag mx (magnetic dipole moment)\n",nc,nc+1); nc+=2;
     fprintf(f,"# %02i,%02i real,imag my \n",nc,nc+1); nc+=2;
     fprintf(f,"# %02i,%02i real,imag mz \n",nc,nc+1); nc+=2;
     fprintf(f,"#\n");
   };
  fclose(f);

  /***************************************************************/
  /* fetch the moments *******************************************/
  /***************************************************************/
  HMatrix *PM = G->GetDipoleMoments(Omega, KN);

  /***************************************************************/
  /* write data to file                                          */
  /***************************************************************/
  f=fopen(CartesianMomentFile,"a");
  cdouble TotalMoments[6]={0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  for (int ns=0; ns<G->NumSurfaces; ns++)
   { fprintf(f,"%s ",z2s(Omega));
     fprintVec(f,kBloch,G->LDim);
     fprintf(f,"%s ",G->Surfaces[ns]->Label);
     for(int Mu=0; Mu<6; Mu++)
      { fprintf(f,"%s ",CD2S(PM->GetEntry(ns,Mu),"%.8e %.8e "));
        TotalMoments[Mu]+=PM->GetEntry(ns,Mu);
      };
     fprintf(f,"\n");
   };

  if (G->NumSurfaces>1)
   { 
     fprintf(f,"%s ",z2s(Omega));
     fprintVec(f,kBloch,G->LDim);
     fprintf(f,"Total ");
     for(int Mu=0; Mu<6; Mu++)
      fprintf(f,"%s ",CD2S(TotalMoments[Mu],"%.8e %.8e "));
     fprintf(f,"\n");
   };
  fclose(f);

  delete PM;

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteSphericalMoments(RWGGeometry *G, HVector *KN,
                           cdouble Omega, int LMax,
                           char *SphericalMomentFile)
{
  /***************************************************************/
  /* write file preamble if necessary ****************************/
  /***************************************************************/
  FILE *f=fopen(SphericalMomentFile,"r");
  if (!f)
   { f=fopen(SphericalMomentFile,"w");
     if (!f) 
      { Warn("could not open file %s (skipping spherical moments)...",SphericalMomentFile);
        return;
      };
     fprintf(f,"# scuff-spectrum run on %s (%s)\n",GetHostName(),GetTimeString());
     fprintf(f,"# columns: \n");
     fprintf(f,"# 1,2   re,im omega (eigenfrequency)\n");
     fprintf(f,"# 3,4   L, M (spherical-wave indices)\n");
     fprintf(f,"# 5,6   magnitude@phase(in degrees) of a^M_{LM} (M-type spherical moment)\n");
     fprintf(f,"# 7,8   magnitude@phase(in degrees) of a^N_{LM} (N-type spherical moment)\n");
   };
  fclose(f);

  /***************************************************************/
  /* fetch spherical moments *************************************/
  /***************************************************************/
  HVector *aLM=GetSphericalMoments(G, Omega, LMax, KN);

  /***************************************************************/
  /* write spherical moments to file                             */
  /***************************************************************/
  f=fopen(SphericalMomentFile,"a");
  fprintf(f,"\n\n# Spherical multipole moments at Omega=%s: \n",CD2S(Omega));
  int NumLMs = (LMax+1)*(LMax+1);

  for(int L=1, nlm=1; L<=LMax; L++)
   for(int M=-L; M<=L; M++, nlm++)
    { cdouble MMoment = aLM->GetEntry(2*nlm+0);
      cdouble NMoment = aLM->GetEntry(2*nlm+1);
      fprintf(f,"%e %e %3i %+3i  %.4f@%+3i   %.4f@+%+3i \n",
               real(Omega), imag(Omega), L, M, 
               abs(MMoment), (int)round(arg(MMoment)*180.0/M_PI),
               abs(NMoment), (int)round(arg(NMoment)*180.0/M_PI)
             );
    };
  fclose(f);
}
