#ifndef DiagnosticProbe_H
#define DiagnosticProbe_H

#include <cmath>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#include <hdf5.h>

#include "Tools.h"

#include "Species.h"
#include "Interpolator.h"
#include "Particles.h"

class PicParams;
class SmileiMPI;
class Patch;
class DiagParams;
class ElectroMagn;
class Field2D;

//! this class holds the point probe
class DiagnosticProbe {
    
public:
    
    //! the creator need both sim parameters params and the diagnostic parameter diagParams
    DiagnosticProbe(PicParams &params, DiagParams &diagParams, Patch* patch);
    
    ~DiagnosticProbe();
    
    //! run all probes
    void run(unsigned int timestep, ElectroMagn* EMfields, Interpolator* interp);
    
    //! return name of the probe based on its number
    std::string probeName(int p);

    //! function to close the file
    void close();

    //! vector containing the timesteps at which calculate each probe
    std::vector<unsigned int> every;

protected:
    
    // rank of the cpu (from smpi) -> patch->hindex
    const unsigned int cpuRank;
    
    //! fake particles acting as probes
    std::vector<Particles> probeParticles;
    
    //! each probe will be associated with a proc
    std::vector<std::vector<int> >probeId;

    //! each probe will write in a buffer
    std::vector< Field2D* > probesArray;
    std::vector< int > probesStart;
    int nProbeTot;
    int nDim;
    
    //! E local fields for the projector
    LocalFields Eloc_fields;
    //! B local fields for the projector
    LocalFields Bloc_fields;
    
    //! J local fields for the projector
    LocalFields Jloc_fields;
    
    //! memory size of a probe should be 6 = Exyz + Bxyz
    const int probeSize;
    
    //! hdf5 file ID
    hid_t fileId;
    
};
#endif
