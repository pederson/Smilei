
#include "Patch.h"

#include <cstdlib>
#include <iostream>
#include <iomanip>

#include "Hilbert_functions.h"
#include "PatchesFactory.h"
#include "Species.h"
#include "Particles.h"
#include "SmileiIOFactory.h"

using namespace std;

//int buildtag(int send, int recv);

Patch::Patch(PicParams& params, DiagParams &diag_params, LaserParams& laser_params, SmileiMPI* smpi, unsigned int ipatch, unsigned int n_moved) {


//Neighborhood definition in 2D:
//   
//   Y axis
//   ^
//   |   6       7          8
//   |   3    4(self)       5
//       0       1          2    --> X axis

        int xcall, ycall;
        hindex = ipatch;
#ifdef _DEBUGPATCH
	std::cout << smpi->getRank() << ", mypatch is : " << hindex << std::endl;        
#endif
	//int nDims = params.nDim_field;
        nDim_fields_ = params.nDim_field;
        int nDims = 2;
        Pcoordinates.resize(nDims);
        if ( params.geometry == "1d3v" ) {
            Pcoordinates[0] = hindex;
            Pcoordinates[1] = 0;
        }
        else if ( params.geometry == "2d3v" ) {
            Pcoordinates.resize(2);
            generalhilbertindexinv(params.mi[0], params.mi[1], &Pcoordinates[0], &Pcoordinates[1], hindex);

	    /////////////////////////////////////
	    //  Define local domain
	    /////////////////////////////////////

        }
        else {
            Pcoordinates.resize(3);
            generalhilbertindexinv(params.mi[0], params.mi[1], params.mi[2], &Pcoordinates[0], &Pcoordinates[1], &Pcoordinates[2], hindex);
        }

	//std::cout << "Coordonnées de " << ipatch << " : " << Pcoordinates[0] << " " << Pcoordinates[1] << std::endl;
	nbNeighbors_ = 2;
	neighbor_.resize(nDim_fields_);
	corner_neighbor_.resize(params.nDim_field);
	for ( int iDim = 0 ; iDim < nDim_fields_ ; iDim++ ) {
	    neighbor_[iDim].resize(2,MPI_PROC_NULL);
	    corner_neighbor_[iDim].resize(2,MPI_PROC_NULL);
	}
	MPI_neighbor_.resize(nDim_fields_);
	for ( int iDim = 0 ; iDim < nDim_fields_; iDim++ ) {
	    MPI_neighbor_[iDim].resize(2,MPI_PROC_NULL);
	}

        xcall = Pcoordinates[0]-1;
        ycall = Pcoordinates[1];
	if (params.bc_em_type_long=="periodic" && xcall < 0) xcall += (1<<params.mi[0]);
	neighbor_[0][0] = generalhilbertindex( params.mi[0], params.mi[1], xcall, ycall);
        xcall = Pcoordinates[0]+1;
	if (params.bc_em_type_long=="periodic" && xcall >= (1<<params.mi[0])) xcall -= (1<<params.mi[0]);
	neighbor_[0][1] = generalhilbertindex( params.mi[0], params.mi[1], xcall, ycall);

	if (params.nDim_field>1) {
	    xcall = Pcoordinates[0];
	    ycall = Pcoordinates[1]-1;
	    if (params.bc_em_type_trans=="periodic" && ycall < 0) ycall += (1<<params.mi[1]);
	    neighbor_[1][0] = generalhilbertindex( params.mi[0], params.mi[1], xcall, ycall);
	    ycall = Pcoordinates[1]+1;
	    if (params.bc_em_type_trans=="periodic" && ycall >= (1<<params.mi[1])) ycall -= (1<<params.mi[1]);
	    neighbor_[1][1] = generalhilbertindex( params.mi[0], params.mi[1], xcall, ycall);

	    xcall = Pcoordinates[0]+1;
	    if (params.bc_em_type_long=="periodic" && xcall >= (1<<params.mi[0])) xcall -= (1<<params.mi[0]);
	    corner_neighbor_[1][1] = generalhilbertindex( params.mi[0], params.mi[1], xcall, ycall);
	    xcall = Pcoordinates[0]-1;
	    if (params.bc_em_type_long=="periodic" && xcall < 0) xcall += (1<<params.mi[0]);
	    corner_neighbor_[0][1] = generalhilbertindex( params.mi[0], params.mi[1], xcall, ycall);
	    ycall = Pcoordinates[1]-1;
	    if (params.bc_em_type_trans=="periodic" && ycall < 0) ycall += (1<<params.mi[1]);
	    corner_neighbor_[0][0] = generalhilbertindex( params.mi[0], params.mi[1], xcall, ycall);
	    xcall = Pcoordinates[0]+1;
	    if (params.bc_em_type_long=="periodic" && xcall >= (1<<params.mi[0])) xcall -= (1<<params.mi[0]);
	    corner_neighbor_[1][0] = generalhilbertindex( params.mi[0], params.mi[1], xcall, ycall);
	}

	updateMPIenv(smpi);

	//createType(params);

	
	//std::cout << "Voisin dir 0 : " << ipatch << " : " <<  neighbor_[0][0] << " " <<  neighbor_[0][1] << std::endl;
	//std::cout << "Voisin dir 1 : " << ipatch << " : " <<  neighbor_[1][0] << " " <<  neighbor_[1][1] << std::endl;

	min_local.resize(params.nDim_field, 0.);
	max_local.resize(params.nDim_field, 0.);
	cell_starting_global_index.resize(params.nDim_field, 0);
	for (int i = 0 ; i<params.nDim_field ; i++) {
	    min_local[i] = Pcoordinates[i]*params.n_space[i]*params.cell_length[i];
	    max_local[i] = min_local[i] + params.n_space[i]*params.cell_length[i];
	    cell_starting_global_index[i] += Pcoordinates[i]*params.n_space[i];
	    cell_starting_global_index[i] -= params.oversize[i];
	}

	cell_starting_global_index[0] += n_moved;
	min_local[0] += n_moved*params.cell_length[0];
	max_local[0] += n_moved*params.cell_length[0];

	vecSpecies = SpeciesFactory::createVector(params, this);

	/* // + min_loc/cell_index(ref smpi,  & sort) // OK through this */
	// + new n_space -> in PatchFactory
	// patchID : ok through coord
	// create Pos : OK

	// -> partBoundCond : min/max_loc (smpi)
	EMfields   = ElectroMagnFactory::create(params, laser_params, this);
	// + patchId + new n_space (now = params by smpi) + BC
	// -> Neighbors to define !!
	
	Interp     = InterpolatorFactory::create(params, this);               // + patchId -> idx_domain_begin (now = ref smpi)
	Proj       = ProjectorFactory::create(params, this);                  // + patchId -> idx_domain_begin (now = ref smpi)

	Diags = new Diagnostic(params,diag_params, this);

	sio = SmileiIOFactory::create(params, diag_params, this);
	
};

void Patch::updateMPIenv(SmileiMPI* smpi)
{
    for (int iDim = 0 ; iDim < nDim_fields_ ; iDim++)
	for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++)
	    MPI_neighbor_[iDim][iNeighbor] = smpi->hrank(neighbor_[iDim][iNeighbor]);
    MPI_me_ = smpi->smilei_rk;


#ifdef _PATCH_DEBUG
	cout << "\n\tPatch Corner decomp : " << corner_neighbor_[0][1] << "\t" << neighbor_[1][1]  << "\t" << corner_neighbor_[1][1] << endl;
	cout << "\tPatch Corner decomp : " << neighbor_[0][0] << "\t" << hindex << "\t" << neighbor_[0][1] << endl;
	cout << "\tPatch Corner decomp : " << corner_neighbor_[0][0] << "\t" << neighbor_[1][0]  << "\t" << corner_neighbor_[1][0] << endl;

	cout << "\n\tMPI Corner decomp : " << "MPI_PROC_NULL" << "\t" << MPI_neighbor_[1][1]  << "\t" << "MPI_PROC_NULL" << endl;
	cout << "\tMPI Corner decomp : " << MPI_neighbor_[0][0] << "\t" << smpi->getRank() << "\t" << MPI_neighbor_[0][1] << endl;
	cout << "\tMPI Corner decomp : " << "MPI_PROC_NULL" << "\t" << MPI_neighbor_[1][0]  << "\t" << "MPI_PROC_NULL" << endl;
#endif

}


void Patch::dynamics(double time_dual, PicParams &params, SimWindow* simWindow, int diag_flag)
{
    for (unsigned int ispec=0 ; ispec<params.n_species; ispec++) {
	if ( vecSpecies[ispec]->isProj(time_dual, simWindow) || diag_flag  ){    
	    vecSpecies[ispec]->dynamics(time_dual, ispec, EMfields, Interp, Proj, params, diag_flag);
	}
    }

}

void Patch::initExchParticles(SmileiMPI* smpi, int ispec, PicParams& params, int tnum, int iDim, VectorPatch * vecPatch)
{
    Particles &cuParticles = (*vecSpecies[ispec]->particles);

    for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++) {
	vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][iNeighbor].initialize(0,cuParticles.dimension());
	vecSpecies[ispec]->specMPI.patchVectorSend[iDim][iNeighbor].initialize(0,cuParticles.dimension());
    }

    
    std::vector< std::vector<int> >* indexes_of_particles_to_exchange_per_thd = &vecSpecies[ispec]->indexes_of_particles_to_exchange_per_thd;
    std::vector<int>*                indexes_of_particles_to_exchange         = &vecSpecies[ispec]->indexes_of_particles_to_exchange;
    
    /********************************************************************************/
    // Build lists of indexes of particle to exchange per neighbor
    // Computed from indexes_of_particles_to_exchange computed during particles' BC
    /********************************************************************************/
    (*indexes_of_particles_to_exchange).clear();
        
    int tmp = 0;
    for (int tid=0 ; tid < (int)indexes_of_particles_to_exchange_per_thd->size() ; tid++)
	tmp += ((*indexes_of_particles_to_exchange_per_thd)[tid]).size();
    (*indexes_of_particles_to_exchange).resize( tmp );
        
    int k=0;
    for (int tid=0 ; tid < (int)indexes_of_particles_to_exchange_per_thd->size() ; tid++) {
	for (int ipart = 0 ; ipart < (int) ((*indexes_of_particles_to_exchange_per_thd)[tid]).size() ; ipart++ ) {
	    (*indexes_of_particles_to_exchange)[k] =  (*indexes_of_particles_to_exchange_per_thd)[tid][ipart] ;
	    k++;
	}
	((*indexes_of_particles_to_exchange_per_thd))[tid].clear();
    }
    sort( (*indexes_of_particles_to_exchange).begin(), (*indexes_of_particles_to_exchange).end() );
        
    int n_part_send = (*indexes_of_particles_to_exchange).size();
    int n_part_recv;
        
    int iPart;
    int n_particles;

    // Define where particles are going 
    diagonalParticles.initialize(0,cuParticles.dimension());
    for (int i=0 ; i<n_part_send ; i++) {
	iPart = (*indexes_of_particles_to_exchange)[i];

	if ( cuParticles.position(iDim,iPart) < min_local[iDim]) { 
	    vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][0].push_back( iPart );
	}
	else if ( cuParticles.position(iDim,iPart) >= max_local[iDim]) { 
	    vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][1].push_back( iPart );
	}
	else if ( !(cuParticles.is_part_in_domain(iPart, this) ) ) {
	    // at the end of exchangeParticles, diagonalParticles will be reinjected 
	    // at the end of cuParticles & indexes_of_particles_to_exchange_per_thd[0] for next iDim
	    cuParticles.cp_particle(iPart, diagonalParticles);
	}
	else { // particle will be deleted, if supp_particle particles still in the domain
	}
    }
    //cout << hindex << " has " << n_part_send << " to send" << endl;
     
    int h0 = (*vecPatch)(0)->hindex;
    /********************************************************************************/
    // Exchange number of particles to exchange to establish or not a communication
    /********************************************************************************/
    for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++) {
	if (neighbor_[iDim][iNeighbor]!=MPI_PROC_NULL) {
	    vecSpecies[ispec]->specMPI.patch_buff_index_send_sz[iDim][iNeighbor] = (vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][iNeighbor]).size();
	    if (is_a_MPI_neighbor(iDim, iNeighbor)) {
		//cout << " send through MPI" << endl;
		//int tag = buildtag( 1, hindex, neighbor_[iDim][iNeighbor]);
		int tag = buildtag( hindex, iDim+1, iNeighbor+3 );
		MPI_Isend( &(vecSpecies[ispec]->specMPI.patch_buff_index_send_sz[iDim][iNeighbor]), 1, MPI_INT, MPI_neighbor_[iDim][iNeighbor], tag, MPI_COMM_WORLD, &(vecSpecies[ispec]->specMPI.patch_srequest[iDim][iNeighbor]) );}
	    else {
		//cout << hindex << " send " << neighbor_[iDim][iNeighbor]- h0 << " with " << vecSpecies[ispec]->specMPI.patch_buff_index_send_sz[iDim][iNeighbor] << " particles" << endl;
		(*vecPatch)( neighbor_[iDim][iNeighbor]- h0 )->vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2] = vecSpecies[ispec]->specMPI.patch_buff_index_send_sz[iDim][iNeighbor];
		//cout << neighbor_[iDim][iNeighbor]- h0 << " initialize with " << vecSpecies[ispec]->specMPI.patch_buff_index_send_sz[iDim][iNeighbor] << " particles" << endl;
		(*vecPatch)( neighbor_[iDim][iNeighbor]- h0 )->vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][(iNeighbor+1)%2].initialize( vecSpecies[ispec]->specMPI.patch_buff_index_send_sz[iDim][iNeighbor], cuParticles.dimension());

	    }
	} // END of Send
	else
	    n_part_send = 0;

	if (neighbor_[iDim][(iNeighbor+1)%2]!=MPI_PROC_NULL) {
	    if (is_a_MPI_neighbor(iDim, (iNeighbor+1)%2)) {
		//cout << " recv through MPI" << endl;
		vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2] = 0;
		//int tag = buildtag( 1, neighbor_[iDim][(iNeighbor+1)%2], hindex);
		int tag = buildtag( neighbor_[iDim][(iNeighbor+1)%2], iDim+1, iNeighbor+3 );
		MPI_Irecv( &(vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2]), 1, MPI_INT, MPI_neighbor_[iDim][(iNeighbor+1)%2], tag, MPI_COMM_WORLD, &(vecSpecies[ispec]->specMPI.patch_rrequest[iDim][(iNeighbor+1)%2]) );
	    }
	}
	else
	    vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2] = 0;

    }

} // initExchParticles(... iDim)

void Patch::initCommParticles(SmileiMPI* smpi, int ispec, PicParams& params, int tnum, int iDim, VectorPatch * vecPatch)
{

    Particles &cuParticles = (*vecSpecies[ispec]->particles);

    /*for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++) {
	vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][iNeighbor].initialize(0,2);
	vecSpecies[ispec]->specMPI.patchVectorSend[iDim][iNeighbor].initialize(0,2);
	}*/


    /********************************************************************************/
    // Wait for end of communications over number of particles
    /********************************************************************************/
    for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++) {
	MPI_Status sstat    [2];
	MPI_Status rstat    [2];
	if (neighbor_[iDim][iNeighbor]!=MPI_PROC_NULL) {
	    if (is_a_MPI_neighbor(iDim, iNeighbor))
		MPI_Wait( &(vecSpecies[ispec]->specMPI.patch_srequest[iDim][iNeighbor]), &(sstat[iNeighbor]) );
	}
	if (neighbor_[iDim][(iNeighbor+1)%2]!=MPI_PROC_NULL) {
	    if (is_a_MPI_neighbor(iDim, (iNeighbor+1)%2))  {
		MPI_Wait( &(vecSpecies[ispec]->specMPI.patch_rrequest[iDim][(iNeighbor+1)%2]), &(rstat[(iNeighbor+1)%2]) );
		if (vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2]!=0) {
		    //cout << hindex << " initialize with " << vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2] << " particles" << endl;
		    vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][(iNeighbor+1)%2].initialize( vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2], cuParticles.dimension());
		}
	    }
	}
    }

    
    /********************************************************************************/
    // Proceed to effective Particles' communications
    /********************************************************************************/

    // Number of properties per particles = nDim_Particles + 3 + 1 + 1
    int nbrOfProp( 7 );
    MPI_Datatype typePartSend, typePartRecv;

    int n_part_send, n_part_recv;

    int h0 = (*vecPatch)(0)->hindex;
    for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++) {
                
	// n_part_send : number of particles to send to current neighbor
	n_part_send = (vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][iNeighbor]).size();
	if ( (neighbor_[iDim][iNeighbor]!=MPI_PROC_NULL) && (n_part_send!=0) ) {
	    double x_max = params.cell_length[iDim]*( params.n_space_global[iDim] );
	    for (int iPart=0 ; iPart<n_part_send ; iPart++) {
		if (smpi->periods_[iDim]==1) {
		    // Enabled periodicity
		    if ( ( iNeighbor==0 ) &&  (Pcoordinates[iDim] == 0 ) &&( cuParticles.position(iDim,vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][iNeighbor][iPart]) < 0. ) ) {
			cuParticles.position(iDim,vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][iNeighbor][iPart])     += x_max;
		    }
		    else if ( ( iNeighbor==1 ) &&  (Pcoordinates[iDim] == params.number_of_patches[iDim]-1 ) && ( cuParticles.position(iDim,vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][iNeighbor][iPart]) >= x_max ) ) {
			cuParticles.position(iDim,vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][iNeighbor][iPart])     -= x_max;
		    }
		}
		cuParticles.cp_particle(vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][iNeighbor][iPart], vecSpecies[ispec]->specMPI.patchVectorSend[iDim][iNeighbor]);
	    }
	    if (is_a_MPI_neighbor(iDim, iNeighbor)) {
		//int tag = buildtag( 2, hindex, neighbor_[iDim][iNeighbor]);
		int tag = buildtag( hindex, iDim+1, iNeighbor+3 );
		typePartSend = smpi->createMPIparticles( &(vecSpecies[ispec]->specMPI.patchVectorSend[iDim][iNeighbor]), nbrOfProp );
		MPI_Isend( &((vecSpecies[ispec]->specMPI.patchVectorSend[iDim][iNeighbor]).position(0,0)), 1, typePartSend, MPI_neighbor_[iDim][iNeighbor], tag, MPI_COMM_WORLD, &(vecSpecies[ispec]->specMPI.patch_srequest[iDim][iNeighbor]) );
		MPI_Type_free( &typePartSend );
	    }
	    else {
		(vecSpecies[ispec]->specMPI.patchVectorSend[iDim][iNeighbor]).cp_particles(0, n_part_send, ((*vecPatch)( neighbor_[iDim][iNeighbor]- h0 )->vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][(iNeighbor+1)%2]), 0);

	    }

	} // END of Send
                
	n_part_recv = vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2];
	if ( (neighbor_[iDim][(iNeighbor+1)%2]!=MPI_PROC_NULL) && (n_part_recv!=0) ) {
	    if (is_a_MPI_neighbor(iDim, (iNeighbor+1)%2)) {
		typePartRecv = smpi->createMPIparticles( &(vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][(iNeighbor+1)%2]), nbrOfProp );
		//int tag = buildtag( 2, neighbor_[iDim][(iNeighbor+1)%2], hindex);
		int tag = buildtag( neighbor_[iDim][(iNeighbor+1)%2], iDim+1 ,iNeighbor+3 );
		MPI_Irecv( &((vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][(iNeighbor+1)%2]).position(0,0)), 1, typePartRecv, MPI_neighbor_[iDim][(iNeighbor+1)%2], tag, MPI_COMM_WORLD, &(vecSpecies[ispec]->specMPI.patch_rrequest[iDim][(iNeighbor+1)%2]) );
		MPI_Type_free( &typePartRecv );
	    }

	} // END of Recv
                
    } // END for iNeighbor


} // initCommParticles(... iDim)

void Patch::finalizeCommParticles(SmileiMPI* smpi, int ispec, PicParams& params, int tnum, int iDim, VectorPatch * vecPatch)
{
    Particles &cuParticles = (*vecSpecies[ispec]->particles);


    std::vector< std::vector<int> >* indexes_of_particles_to_exchange_per_thd = &vecSpecies[ispec]->indexes_of_particles_to_exchange_per_thd;
    std::vector<int>*                indexes_of_particles_to_exchange         = &vecSpecies[ispec]->indexes_of_particles_to_exchange;

    std::vector<int>* cubmin = &vecSpecies[ispec]->bmin;
    std::vector<int>* cubmax = &vecSpecies[ispec]->bmax;

    int nmove,lmove,ii; // local, OK
    int shift[(*cubmax).size()+1];//how much we need to shift each bin in order to leave room for the new particles
    double dbin;
        
    dbin = params.cell_length[0]*params.clrw; //width of a bin.
    for (unsigned int j=0; j<(*cubmax).size()+1 ;j++){
      shift[j]=0;
    }

    int n_part_send, n_part_recv, n_particles;

    /********************************************************************************/
    // Wait for end of communications over Particles
    /********************************************************************************/
    for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++) {
	MPI_Status sstat    [2];
	MPI_Status rstat    [2];
                
	n_part_send = vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][iNeighbor].size();
	n_part_recv = vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2];
                
	if ( (neighbor_[iDim][iNeighbor]!=MPI_PROC_NULL) && (n_part_send!=0) ) {
	    if (is_a_MPI_neighbor(iDim, iNeighbor))
		MPI_Wait( &(vecSpecies[ispec]->specMPI.patch_srequest[iDim][iNeighbor]), &(sstat[iNeighbor]) );
	}
	if ( (neighbor_[iDim][(iNeighbor+1)%2]!=MPI_PROC_NULL) && (n_part_recv!=0) ) {
	    if (is_a_MPI_neighbor(iDim, (iNeighbor+1)%2))
		MPI_Wait( &(vecSpecies[ispec]->specMPI.patch_rrequest[iDim][(iNeighbor+1)%2]), &(rstat[(iNeighbor+1)%2]) );     

	    // In fact diagonalParticles
	    for (int iPart=0 ; iPart<n_part_recv; iPart++ )
		if ( !(vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][(iNeighbor+1)%2]).is_part_in_domain(iPart, this) )
		    (vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][(iNeighbor+1)%2]).cp_particle(iPart, diagonalParticles);
	    for (int iPart=n_part_recv-1 ; iPart>=0; iPart-- ) {
		if ( !(vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][(iNeighbor+1)%2]).is_part_in_domain(iPart, this) ) {
		    (vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][(iNeighbor+1)%2]).erase_particle(iPart);
		    vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][(iNeighbor+1)%2]--;
		}
	    }

	}
    }


    /********************************************************************************/
    // Clean lists of indexes of particle to exchange per neighbor
    /********************************************************************************/
    for (int i=0 ; i<nbNeighbors_ ; i++) {
	vecSpecies[ispec]->specMPI.patch_buff_index_send[iDim][i].clear();
    }
            
        
    /********************************************************************************/
    // Delete Particles included in buff_send/buff_recv
    /********************************************************************************/
    cleanup_sent_particles(ispec, indexes_of_particles_to_exchange);


    // Delete useless Particles
    //Theoretically, not even necessary to do anything as long you use bmax as the end of your iterator on particles.
    //Nevertheless, you might want to free memory and have the actual number of particles
    //really equal to the size of the vector. So we do:
    cuParticles.erase_particle_trail((*cubmax).back());
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



    for (unsigned int j=0; j<(*cubmax).size()+1 ;j++){
	shift[j]=0;
    }

    //********************************************************************************/
    // Copy newly arrived particles back to the vector
    // WARNING: very different behaviour depending on which dimension particles are coming from.
    /********************************************************************************/
    //We first evaluate how many particles arrive in each bin.
    if (iDim==1) {
	//1) Count particles coming from south and north
	for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++) {
	    n_part_recv = vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][iNeighbor];
	    for (unsigned int j=0; j<n_part_recv ;j++){
		ii = int((vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][iNeighbor].position(0,j)-min_local[0])/dbin);//bin in which the particle goes.
		shift[ii+1]++; // It makes the next bins shift.
	    }
	}
    }
    if (iDim==0) {
	//2) Add particles coming from west and east
	shift[1] += vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][0];//Particles coming from south all go to bin 0 and shift all the other bins.
	shift[(*cubmax).size()] += vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][1];//Used only to count the total number of particles arrived.
    }


    //Evaluation of the necessary shift of all bins.
    //Must be done sequentially
    for (unsigned int j=1; j<(*cubmax).size()+1;j++){ //bin 0 is not shifted.Last element of shift stores total number of arriving particles.
	shift[j]+=shift[j-1];
    }
    //Make room for new particles
    cuParticles.initialize( cuParticles.size()+shift[(*cubmax).size()], cuParticles.dimension() );
        
    //Shift bins, must be done sequentially
    for (unsigned int j=(*cubmax).size()-1; j>=1; j--){
	n_particles = (*cubmax)[j]-(*cubmin)[j]; //Nbr of particle in this bin
	nmove = min(n_particles,shift[j]); //Nbr of particles to move
	lmove = max(n_particles,shift[j]); //How far particles must be shifted
	if (nmove>0) cuParticles.overwrite_part((*cubmin)[j], (*cubmin)[j]+lmove, nmove);
	(*cubmin)[j] += shift[j];
	(*cubmax)[j] += shift[j];
    }
        
    //Space has been made now to write the arriving particles into the correct bins
    //iDim == 0  is the easy case, when particles arrive either in first or last bin.
    if (iDim==0) {
	for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++) {
	    n_part_recv = vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][iNeighbor];
	    if ( (neighbor_[0][iNeighbor]!=MPI_PROC_NULL) && (n_part_recv!=0) ) {
		ii = iNeighbor*((*cubmax).size()-1);//0 if iNeighbor=0(particles coming from West) and (*cubmax).size()-1 otherwise.
		vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][iNeighbor].overwrite_part(0, cuParticles,(*cubmax)[ii],n_part_recv);
		(*cubmax)[ii] += n_part_recv ;
	    }
	}
    }
    //iDim == 1) is the difficult case, when particles can arrive in any bin.
    if (iDim==1) {
	for (int iNeighbor=0 ; iNeighbor<nbNeighbors_ ; iNeighbor++) {
	    n_part_recv = vecSpecies[ispec]->specMPI.patch_buff_index_recv_sz[iDim][iNeighbor];
	    if ( (neighbor_[1][iNeighbor]!=MPI_PROC_NULL) && (n_part_recv!=0) ) {
		for(unsigned int j=0; j<n_part_recv; j++){
		    ii = int((vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][iNeighbor].position(0,j)-min_local[0])/dbin);//bin in which the particle goes.
		    vecSpecies[ispec]->specMPI.patchVectorRecv[iDim][iNeighbor].overwrite_part(j, cuParticles,(*cubmax)[ii]);
		    (*cubmax)[ii] ++ ;
		}
	    }
	}
    }

    // Inject corner particles at the end of the list, update bmax
    //if (iDim==cuParticles.dimension()-1) cout << "Number of diag particles " << diagonalParticles.size() << endl;
    for (int iPart = 0 ; iPart<diagonalParticles.size() ; iPart++) {
	diagonalParticles.cp_particle(iPart, cuParticles);
	(*indexes_of_particles_to_exchange_per_thd)[0].push_back(cuParticles.size()-1);
	(*cubmax)[(*cubmax).size()-1]++;
    }

} // finalizeCommParticles(... iDim)


void Patch::cleanup_sent_particles(int ispec, std::vector<int>* indexes_of_particles_to_exchange)
{
    /********************************************************************************/
    // Delete Particles included in the index of particles to exchange. Assumes indexes are sorted.
    /********************************************************************************/
    int ii, iPart;
    std::vector<int>* cubmin = &vecSpecies[ispec]->bmin;
    std::vector<int>* cubmax = &vecSpecies[ispec]->bmax;
    Particles &cuParticles = (*vecSpecies[ispec]->particles);

    
    // Push lost particles at the end of bins
    for (unsigned int ibin = 0 ; ibin < (*cubmax).size() ; ibin++ ) {
	ii = (*indexes_of_particles_to_exchange).size()-1;
	if (ii >= 0) { // Push lost particles to the end of the bin
	    iPart = (*indexes_of_particles_to_exchange)[ii];
	    while (iPart >= (*cubmax)[ibin] && ii > 0) {
		ii--;
		iPart = (*indexes_of_particles_to_exchange)[ii];
	    }
	    while (iPart == (*cubmax)[ibin]-1 && iPart >= (*cubmin)[ibin] && ii > 0) {
		(*cubmax)[ibin]--;
		ii--;
		iPart = (*indexes_of_particles_to_exchange)[ii];
	    }
	    while (iPart >= (*cubmin)[ibin] && ii > 0) {
		cuParticles.overwrite_part((*cubmax)[ibin]-1, iPart );
		(*cubmax)[ibin]--;
		ii--;
		iPart = (*indexes_of_particles_to_exchange)[ii];
	    }
	    if (iPart >= (*cubmin)[ibin] && iPart < (*cubmax)[ibin]) { //On traite la dernière particule (qui peut aussi etre la premiere)
		cuParticles.overwrite_part((*cubmax)[ibin]-1, iPart );
		(*cubmax)[ibin]--;
	    }
	}
    }


    //Shift the bins in memory
    //Warning: this loop must be executed sequentially. Do not use openMP here.
    for (int unsigned ibin = 1 ; ibin < (*cubmax).size() ; ibin++ ) { //First bin don't need to be shifted
	ii = (*cubmin)[ibin]-(*cubmax)[ibin-1]; // Shift the bin in memory by ii slots.
	iPart = min(ii,(*cubmax)[ibin]-(*cubmin)[ibin]); // Number of particles we have to shift = min (Nshift, Nparticle in the bin)
	if(iPart > 0) cuParticles.overwrite_part((*cubmax)[ibin]-iPart,(*cubmax)[ibin-1],iPart);
	(*cubmax)[ibin] -= ii;
	(*cubmin)[ibin] = (*cubmax)[ibin-1];
    }

}
