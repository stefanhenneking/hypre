/*BHEADER**********************************************************************
 * Copyright (c) 2008,  Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * This file is part of HYPRE.  See file COPYRIGHT for details.
 *
 * HYPRE is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * $Revision$
 ***********************************************************************EHEADER*/

#include "_hypre_struct_ls.h"
#include "pfmg.h"

/*--------------------------------------------------------------------------
 * hypre_PFMGCreateRAPOp
 *
 *   Wrapper for 2 and 3d CreateRAPOp routines which set up new coarse
 *   grid structures.
 *
 *   The parameter rap_type controls which lower level routines are
 *   used.
 *      rap_type = 0   Use optimized code for computing Galerkin operators
 *                     for special, common stencil patterns: 5 & 9 pt in
 *                     2d and 7, 19 & 27 in 3d.
 *      rap_type = 1   Use PARFLOW formula for coarse grid operator. Used
 *                     only with 5pt in 2d and 7pt in 3d.
 *      rap_type = 2   General purpose Galerkin code.
 *--------------------------------------------------------------------------*/
 
hypre_StructMatrix *
hypre_PFMGCreateRAPOp( hypre_StructMatrix *R,
                       hypre_StructMatrix *A,
                       hypre_StructMatrix *P,
                       hypre_StructGrid   *coarse_grid,
                       HYPRE_Int           cdir,
                       HYPRE_Int           rap_type    )
{
   hypre_StructMatrix    *RAP=NULL;
   hypre_StructStencil   *stencil;
   HYPRE_Int              P_stored_as_transpose = 0;
   HYPRE_Int              constant_coefficient;

   stencil = hypre_StructMatrixStencil(A);

   if (rap_type == 0)
   {
      switch (hypre_StructStencilNDim(stencil)) 
      {
         case 2:
            RAP = hypre_PFMG2CreateRAPOp(R ,A, P, coarse_grid, cdir);
            break;
    
         case 3:
            RAP = hypre_PFMG3CreateRAPOp(R ,A, P, coarse_grid, cdir);
            break;
      } 
   }

   else if (rap_type == 1)
   {
      switch (hypre_StructStencilNDim(stencil)) 
      {
         case 2:
            RAP =  hypre_PFMGCreateCoarseOp5(R ,A, P, coarse_grid, cdir);
            break;
    
         case 3:
            RAP =  hypre_PFMGCreateCoarseOp7(R ,A, P, coarse_grid, cdir);
            break;
      } 
   }
   else if (rap_type == 2)
   {
      RAP = hypre_SemiCreateRAPOp(R ,A, P, coarse_grid, cdir,
                                  P_stored_as_transpose);
   }


   constant_coefficient = hypre_StructMatrixConstantCoefficient(A);
   if ( constant_coefficient==2 && rap_type==0 )
   {
      /* A has variable diagonal, so, in the Galerkin case, P (and R) is
         entirely variable coefficient.  Thus RAP will be variable coefficient */
      hypre_StructMatrixSetConstantCoefficient( RAP, 0 );
   }
   else
   {
      hypre_StructMatrixSetConstantCoefficient( RAP, constant_coefficient );
   }

   return RAP;
}

/*--------------------------------------------------------------------------
 * hypre_PFMGSetupRAPOp
 *
 * Wrapper for 2 and 3d, symmetric and non-symmetric routines to calculate
 * entries in RAP. Incomplete error handling at the moment. 
 *
 *   The parameter rap_type controls which lower level routines are
 *   used.
 *      rap_type = 0   Use optimized code for computing Galerkin operators
 *                     for special, common stencil patterns: 5 & 9 pt in
 *                     2d and 7, 19 & 27 in 3d.
 *      rap_type = 1   Use PARFLOW formula for coarse grid operator. Used
 *                     only with 5pt in 2d and 7pt in 3d.
 *      rap_type = 2   General purpose Galerkin code.
 *--------------------------------------------------------------------------*/
 
HYPRE_Int
hypre_PFMGSetupRAPOp( hypre_StructMatrix *R,
                      hypre_StructMatrix *A,
                      hypre_StructMatrix *P,
                      HYPRE_Int           cdir,
                      hypre_Index         cindex,
                      hypre_Index         cstride,
                      HYPRE_Int           rap_type,
                      hypre_StructMatrix *Ac      )
{
   HYPRE_Int              P_stored_as_transpose = 0;
   hypre_StructStencil   *stencil;
   hypre_StructMatrix    *Ac_tmp;

#if defined(HYPRE_MEMORY_GPU)
   HYPRE_Int data_location_A = hypre_StructGridDataLocation(hypre_StructMatrixGrid(A));
   HYPRE_Int data_location_Ac = hypre_StructGridDataLocation(hypre_StructMatrixGrid(Ac));
   
   if ( data_location_A != data_location_Ac )
   {
      Ac_tmp = hypre_PFMGCreateRAPOp(R, A, P, hypre_StructMatrixGrid(Ac), cdir, rap_type);
      hypre_StructMatrixSymmetric(Ac_tmp) = hypre_StructMatrixSymmetric(Ac);
      hypre_StructMatrixConstantCoefficient(Ac_tmp) = hypre_StructMatrixConstantCoefficient(Ac);
      hypre_StructGridDataLocation(hypre_StructMatrixGrid(Ac)) = data_location_A;
      HYPRE_StructMatrixInitialize(Ac_tmp);
   }
   else
   {
      Ac_tmp = Ac;
   }
#else
   Ac_tmp = Ac;
#endif
   stencil = hypre_StructMatrixStencil(A);

   if (rap_type == 0)
   {
      switch (hypre_StructStencilNDim(stencil)) 
      {
         case 2:
            /*--------------------------------------------------------------------
             *    Set lower triangular (+ diagonal) coefficients
             *--------------------------------------------------------------------*/
            hypre_PFMG2BuildRAPSym(A, P, R, cdir, cindex, cstride, Ac_tmp);

            /*--------------------------------------------------------------------
             *    For non-symmetric A, set upper triangular coefficients as well
             *--------------------------------------------------------------------*/
            if(!hypre_StructMatrixSymmetric(A))
               hypre_PFMG2BuildRAPNoSym(A, P, R, cdir, cindex, cstride, Ac_tmp);

            break;

         case 3:

            /*--------------------------------------------------------------------
             *    Set lower triangular (+ diagonal) coefficients
             *--------------------------------------------------------------------*/
            hypre_PFMG3BuildRAPSym(A, P, R, cdir, cindex, cstride, Ac_tmp);

            /*--------------------------------------------------------------------
             *    For non-symmetric A, set upper triangular coefficients as well
             *--------------------------------------------------------------------*/
            if(!hypre_StructMatrixSymmetric(A))
               hypre_PFMG3BuildRAPNoSym(A, P, R, cdir, cindex, cstride, Ac_tmp);

            break;
      } 
   }

   else if (rap_type == 1)
   {
      switch (hypre_StructStencilNDim(stencil)) 
      {
         case 2:
            hypre_PFMGBuildCoarseOp5(A, P, R, cdir, cindex, cstride, Ac_tmp);
            break;

         case 3:
            hypre_PFMGBuildCoarseOp7(A, P, R, cdir, cindex, cstride, Ac_tmp);
            break;
      } 
   }

   else if (rap_type == 2)
   {
      hypre_SemiBuildRAP(A, P, R, cdir, cindex, cstride,
                         P_stored_as_transpose, Ac_tmp);
   }
   
   hypre_StructMatrixAssemble(Ac_tmp);
#if defined(HYPRE_MEMORY_GPU)   
   if ( data_location_A != data_location_Ac )
   {
      hypre_DataCopyFromData(hypre_StructMatrixDataConst(Ac),hypre_StructMatrixData(Ac_tmp),HYPRE_Complex,hypre_StructMatrixDataSize(Ac_tmp));
      hypre_exec_policy = data_location_Ac;
      hypre_StructGridDataLocation(hypre_StructMatrixGrid(Ac)) = data_location_Ac;
      hypre_StructMatrixAssemble(Ac);
      hypre_exec_policy = data_location_A;
      hypre_StructMatrixDestroy(Ac_tmp);
   }
#endif

   return hypre_error_flag;
}

