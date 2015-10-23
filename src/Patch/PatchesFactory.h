#ifndef PATCHESFACTORY_H
#define PATCHESFACTORY_H

#include "VectorPatch.h"
#include "Patch1D.h"
#include "Patch2D.h"

#include "Tools.h"

class PatchesFactory {
public:
    static Patch* create(PicParams& params, DiagParams& diag_params, LaserParams& laser_params, SmileiMPI* smpi, unsigned int  ipatch) {
	Patch* patch;
	if (params.geometry == "1d3v")
	    patch = new Patch1D(params, diag_params, laser_params, smpi, ipatch, 0);
	else if (params.geometry == "2d3v" )
	    patch = new Patch2D(params, diag_params, laser_params, smpi, ipatch, 0);
	else 
	    ERROR( "Unknwon parameters : " << params.geometry );
	patch->createType(params);
        return patch;
    }

    static Patch* create(PicParams& params, DiagParams& diag_params, LaserParams& laser_params, SmileiMPI* smpi, unsigned int  ipatch, unsigned int n_moved) {
	Patch* patch;
	if (params.geometry == "1d3v")
	    patch = new Patch1D(params, diag_params, laser_params, smpi, ipatch, n_moved);
	else if (params.geometry == "2d3v" )
	    patch = new Patch2D(params, diag_params, laser_params, smpi, ipatch, n_moved);
	else 
	    ERROR( "Unknwon parameters : " << params.geometry );
	patch->createType(params);
        return patch;
    }

    static VectorPatch createVector(PicParams& params, DiagParams& diag_params, LaserParams& laser_params, SmileiMPI* smpi) {
        VectorPatch vecPatches;

	// Compute npatches (1 is std MPI behavior)
	unsigned int npatches, firstpatch;
        npatches = smpi->patch_count[smpi->getRank()];// Number of patches owned by current MPI process.
        firstpatch = 0;
        for (unsigned int impi = 0 ; impi < smpi->getRank() ; impi++) {
            firstpatch += smpi->patch_count[impi];
        }
#ifdef _DEBUGPATCH
	std::cout << smpi->getRank() << ", nPatch = " << npatches << " - starting at " << firstpatch << std::endl;        
#endif
	// Modified to test Patch integration

	//std::cout << "n_space : " << params.n_space[0] << " " << params.n_space[1] << std::endl;
	//std::cout << "n_patch : " << params.number_of_patches[0] << " " << params.number_of_patches[1] << std::endl;

	if (smpi->isMaster())
	    std::cout << "Patch : n_space : " << params.n_space[0] << " " << params.n_space[1] << std::endl;	

        // create species
        vecPatches.resize(npatches);
        for (unsigned int ipatch = 0 ; ipatch < npatches ; ipatch++) {
	    vecPatches.patches_[ipatch] = PatchesFactory::create(params, diag_params, laser_params, smpi, firstpatch + ipatch);
        }
        vecPatches.set_refHindex() ;

        return vecPatches;
    }

};

#endif
