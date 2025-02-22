
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <ostream>
#include <fstream>

#include "CollisionsSingle.h"
#include "SmileiMPI.h"
#include "Field2D.h"
#include "H5.h"
#include "Patch.h"
#include "VectorPatch.h"

using namespace std;


// Calculates the collisions
// The difference with Collisions::collide is that this version
// does not handle more than 1 species on each side,
// but is potentially faster
void CollisionsSingle::collide( Params &params, Patch *patch, int itime, vector<Diagnostic *> &localDiags )
{

    vector<unsigned int> index1;
    unsigned int npairs; // number of pairs of macro-particles
    unsigned int np1, np2; // numbers of macro-particles in each species
    double n1, n2, n12, n123, n223; // densities of particles
    unsigned int i1=0, i2, N2max, first_index1, first_index2;
    Species   *s1, *s2;
    Particles *p1=NULL, *p2;
    double m12, coeff3, coeff4, logL, s, ncol, debye2=0.;
    
    s1 = patch->vecSpecies[species_group1_[0]];
    s2 = patch->vecSpecies[species_group2_[0]];
    
    bool debug = ( debug_every_ > 0 && itime % debug_every_ == 0 ); // debug only every N timesteps
    
    if( debug ) {
        ncol = 0.;
        smean_       = 0.;
        logLmean_    = 0.;
        //temperature = 0.;
    }
    
    // Loop bins of particles (typically, cells, but may also be clusters)
    unsigned int nbin = patch->vecSpecies[0]->first_index.size();
    for( unsigned int ibin = 0 ; ibin < nbin ; ibin++ ) {
    
        // get number of particles for all necessary species
        np1 = s1->last_index[ibin] - s1->first_index[ibin];
        np2 = s2->last_index[ibin] - s2->first_index[ibin];
        // skip to next bin if no particles
        if( np1==0 || np2==0 ) {
            continue;
        }
        // Ensure species 1 has more macro-particles
        if( np2 > np1 ) {
            swap( s1, s2 );
            swap( np1, np2 );
        }
        first_index1 = s1->first_index[ibin];
        first_index2 = s2->first_index[ibin];
        p1 = s1->particles;
        p2 = s2->particles;
        
        // Set the debye length
        if( Collisions::debye_length_required ) {
            debye2 = patch->debye_length_squared[ibin];
        }
        
        // Shuffle particles of species 1 to have random pairs
        // In the case of collisions within one species
        if( intra_collisions_ ) {
            if( np1 < 2 ) continue;
            npairs = ( int ) ceil( ( ( double )np1 )/2. ); // half as many pairs as macro-particles
            N2max = np1 - npairs; // number of not-repeated particles (in second half only)
            first_index2 += npairs;
            // In the case of collisions between two species
        } else {
            npairs = np1; // as many pairs as macro-particles in species 1 (most numerous)
            N2max = np2; // number of not-repeated particles (in species 2 only)
        }
        // Shuffle one particle in each pair
        index1.resize( npairs );
        for( unsigned int i=0; i<npairs; i++ ) {
            index1[i] = first_index1 + i;
        }
        for( unsigned int i=npairs; i>1; i-- ) {
            unsigned int p = patch->xorshift32() % i;
            swap( index1[i-1], index1[p] );
        }
        p1->swap_parts( index1 ); // exchange particles along the cycle defined by the shuffle
        
        // Prepare the ionization
        Ionization->prepare1( s1->atomic_number );
        
        // Calculate the densities
        n1  = 0.; // density of species 1
        n2  = 0.; // density of species 2
        n12 = 0.; // "hybrid" density
        for( unsigned int i=first_index1; i<first_index1+npairs; i++ ) {
            n1 += p1->weight( i );
        }
        for( unsigned int i=first_index2; i<first_index2+N2max; i++ ) {
            n2 += p2->weight( i );
        }
        for( unsigned int i=0; i<npairs; i++ ) {
            i1 = first_index1 + i;
            i2 = first_index2 + i%N2max;
            n12 += min( p1->weight( i1 ),  p2->weight( i2 ) );
            Ionization->prepare2( p1, i1, p2, i2, i<N2max );
        }
        if( intra_collisions_ ) {
            n1 += n2;
            n2 = n1;
        }
        
        // Pre-calculate some numbers before the big loop
        double inv_cell_volume = 1./patch->getCellVolume( p1, i1 );
        n1  *= inv_cell_volume;
        n2  *= inv_cell_volume;
        n12 *= inv_cell_volume;
        n123 = pow( n1, 2./3. );
        n223 = pow( n2, 2./3. );
        coeff3 = params.timestep * n1*n2/n12;
        coeff4 = pow( 3.*coeff2_, -1./3. ) * coeff3;
        coeff3 *= coeff2_;
        m12  = s1->mass / s2->mass; // mass ratio
        
        // Prepare the ionization
        Ionization->prepare3( params.timestep, inv_cell_volume );
        
        // Now start the real loop on pairs of particles
        // ----------------------------------------------------
        for( unsigned int i=0; i<npairs; i++ ) {
            i1 = first_index1 + i;
            i2 = first_index2 + i%N2max;
            
            logL = coulomb_log_;
            double U1  = patch->xorshift32() * patch->xorshift32_invmax;
            double U2  = patch->xorshift32() * patch->xorshift32_invmax;
            double phi = patch->xorshift32() * patch->xorshift32_invmax * twoPi;
            
            s = one_collision( p1, i1, s1->mass, p2, i2, m12, coeff1_, coeff2_, coeff3, coeff4, n123, n223, debye2, logL, U1, U2, phi );
            
            // Handle ionization
            Ionization->apply( patch, p1, i1, p2, i2 );
            
            if( debug ) {
                ncol     += 1;
                smean_    += s;
                logLmean_ += logL;
                //temperature += m1 * (sqrt(1.+pow(p1->momentum(0,i1),2)+pow(p1->momentum(1,i1),2)+pow(p1->momentum(2,i1),2))-1.);
            }
            
        } // end loop on pairs of particles
        
    } // end loop on bins
    
    Ionization->finish( params, patch, localDiags );
    
    if( debug && ncol>0. ) {
        smean_    /= ncol;
        logLmean_ /= ncol;
        //temperature /= ncol;
    }
}
