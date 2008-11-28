//  Little cms
//  Copyright (C) 2008 Mozilla Foundation
//
// Permission is hereby granted, free of charge, to any person obtaining 
// a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software 
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION 
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "lcms.h"

/*
 * Helper Macro for allocating precache tables and freeing them
 * appropriately. We need a macro here so we can select the appropriate member
 * of the union.
 */
#define cmsAllocPrecacheTables(aProf, cacheType, unionMemb, nTables, elemSize, nElems) \
{\
       unsigned i, j; \
       for (i = 0; i < nTables; ++i) { \
              aProf->Precache[cacheType]->Impl.unionMemb.Cache[i] = \
                _cmsMalloc(elemSize * nElems); \
              if (aProf->Precache[cacheType]->Impl.unionMemb.Cache[i] == NULL) { \
                     for (j = 0; j < i; ++j) \
                            _cmsFree(aProf->Precache[cacheType]->Impl.unionMemb.Cache[j]); \
                     _cmsFree(aProf->Precache[cacheType]); \
                     aProf->Precache[cacheType] = NULL; \
                     return FALSE; \
              } \
       } \
}

/*
 * Precaches the results specified in Type in a reference-counted table.
 *
 * Returns true if the precache succeeded (or if the information was already
 * precached), false otherwise (including cases where the profile was not
 * precacheable).
 */
LCMSBOOL LCMSEXPORT cmsPrecacheProfile(cmsHPROFILE hProfile, 
                                       LCMSPRECACHETYPE Type) {

       // Locals.
       LPGAMMATABLE GTables[3];
       LCMSBOOL hasGammaTables;
       LPLCMSICCPROFILE Icc = (LPLCMSICCPROFILE) (LPSTR) hProfile;
       L16PARAMS p16;
       unsigned i, j;

       // Input Validation
       CMSASSERT(Type < PRECACHE_TYPE_COUNT);

       /* Do we already have what we need? */
       if (Icc->Precache[Type] != NULL)
              return TRUE;

       /* Determine if we have gamma tables in the profile. */
       hasGammaTables = cmsIsTag(hProfile, icSigRedTRCTag) &&
                        cmsIsTag(hProfile, icSigGreenTRCTag) &&
                        cmsIsTag(hProfile, icSigBlueTRCTag);

       /* Zero Out the Gamma Table Pointers. */
       ZeroMemory(GTables, sizeof(GTables));

       // Create and zero a precache structure
       Icc->Precache[Type] = _cmsMalloc(sizeof(LCMSPRECACHE));
       if (Icc->Precache[Type] == NULL)
              return FALSE;
       ZeroMemory(Icc->Precache[Type], sizeof(LCMSPRECACHE));

       // Grab a Reference to the precache
       PRECACHE_ADDREF(Icc->Precache[Type]);

       // Tag the precache with its type (necessary for freeing)
       Icc->Precache[Type]->Type = Type;

       // Read the Gamma Tables if we need then
       if (IS_LI_REVERSE(Type)) {

              // Read in the reversed Gamma curves
              if (hasGammaTables) {
                     GTables[0] = cmsReadICCGammaReversed(hProfile, icSigRedTRCTag);
                     GTables[1] = cmsReadICCGammaReversed(hProfile, icSigGreenTRCTag);
                     GTables[2] = cmsReadICCGammaReversed(hProfile, icSigBlueTRCTag);
              }

              // Check tables
              if (!GTables[0] || !GTables[1] || !GTables[2]) {
                     _cmsFree(Icc->Precache[Type]);
                     Icc->Precache[Type] = NULL;
                     return FALSE;
              }
       }
       else if (IS_LI_FORWARD(Type)) {

              // Read in the Gamma curves
              if (hasGammaTables) {
                     GTables[0] = cmsReadICCGamma(hProfile, icSigRedTRCTag);
                     GTables[1] = cmsReadICCGamma(hProfile, icSigGreenTRCTag);
                     GTables[2] = cmsReadICCGamma(hProfile, icSigBlueTRCTag);
              }

              // Check tables
              if (!GTables[0] || !GTables[1] || !GTables[2]) {
                     _cmsFree(Icc->Precache[Type]);
                     Icc->Precache[Type] = NULL;
                     return FALSE;
              }
       }

       // Type-Specific Precache Operations
       switch(Type) {

              case CMS_PRECACHE_LI1616_REVERSE:

                     // Allocate the precache tables
                     cmsAllocPrecacheTables(Icc, Type, LI1616_REVERSE, 3, sizeof(WORD), (1 << 16));

                     // Calculate the interpolation parameters
                     cmsCalcL16Params(GTables[0]->nEntries, &p16);

                     // Compute the cache
                     for (i = 0; i < 3; ++i)
                            for (j = 0; j < (1 << 16); ++j)
                                   Icc->Precache[Type]->Impl.LI1616_REVERSE.Cache[i][j] =
                                          cmsLinearInterpLUT16((WORD)j, GTables[i]->GammaTable, &p16);
                     break;

              case CMS_PRECACHE_LI168_REVERSE:

                     // Allocate the precache tables
                     cmsAllocPrecacheTables(Icc, Type, LI168_REVERSE, 3, sizeof(BYTE), (1 << 16));

                     // Calculate the interpolation parameters
                     cmsCalcL16Params(GTables[0]->nEntries, &p16);

                     // Compute the cache
                     for (i = 0; i < 3; ++i)
                            for (j = 0; j < (1 << 16); ++j)
                                   Icc->Precache[Type]->Impl.LI168_REVERSE.Cache[i][j] =
                                          RGB_16_TO_8(cmsLinearInterpLUT16((WORD)j, GTables[i]->GammaTable, &p16));
                     break;

              case CMS_PRECACHE_LI16W_FORWARD:

                     // Allocate the precache tables
                     cmsAllocPrecacheTables(Icc, Type, LI16W_FORWARD, 3, sizeof(Fixed32), (1 << 16));

                     // Calculate the interpolation parameters
                     cmsCalcL16Params(GTables[0]->nEntries, &p16);

                     // Compute the cache
                     for (i = 0; i < 3; ++i)
                            for (j = 0; j < (1 << 16); ++j)
                                   Icc->Precache[Type]->Impl.LI16W_FORWARD.Cache[i][j] =
                                          cmsLinearInterpFixed((WORD)j, GTables[i]->GammaTable, &p16);
                     break;

              case CMS_PRECACHE_LI16F_FORWARD:

                     // Allocate the precache tables
                     cmsAllocPrecacheTables(Icc, Type, LI16F_FORWARD, 3, sizeof(FLOAT), 256);

                     // Calculate the interpolation parameters
                     cmsCalcL16Params(GTables[0]->nEntries, &p16);

                     // Compute the cache
                     for (i = 0; i < 3; ++i)
                            for (j = 0; j < 256; ++j)
                                   Icc->Precache[Type]->Impl.LI16F_FORWARD.Cache[i][j] =
                                          ToFloatDomain(cmsLinearInterpLUT16(RGB_8_TO_16(((BYTE)j)), GTables[i]->GammaTable, &p16));
                     break;

              default:
                     // TODO: change non-critical asserts to CMS warnings
                     CMSASSERT(0); // Not implemented
                     break;
       }

       // Success
       return TRUE;
}

/*
 * Frees a Precache structure.
 *
 * This function is invoked by the refcounting mechanism when the refcount on
 * the precache object drops to zero. If should never be invoked manually.
 */
void cmsPrecacheFree(LPLCMSPRECACHE Cache) {

       // Locals
       unsigned i;

       // Validate Input/State
       CMSASSERT(Cache != NULL);
       CMSASSERT(Cache->RefCount == 0);

       // Type-Specific behavior
       switch(Cache->Type) {

              case CMS_PRECACHE_LI1616_REVERSE:
                     for (i = 0; i < 3; ++i)
                      _cmsFree(Cache->Impl.LI1616_REVERSE.Cache[i]);
                     break;

              case CMS_PRECACHE_LI168_REVERSE:
                     for (i = 0; i < 3; ++i)
                      _cmsFree(Cache->Impl.LI168_REVERSE.Cache[i]);
                     break;

              case CMS_PRECACHE_LI16W_FORWARD:
                     for (i = 0; i < 3; ++i)
                      _cmsFree(Cache->Impl.LI16W_FORWARD.Cache[i]);
                     break;

              case CMS_PRECACHE_LI16F_FORWARD:
                     for (i = 0; i < 3; ++i)
                      _cmsFree(Cache->Impl.LI16F_FORWARD.Cache[i]);
                     break;

              default:
                     // TODO: proper warning
                     CMSASSERT(0); // Bad Type
                     break;
       }

       // Free the structure itself
       _cmsFree(Cache);

}
