
#ifndef COLLISIONSFACTORY_H
#define COLLISIONSFACTORY_H

#include "Collisions.h"
#include "CollisionsSingle.h"
#include "Params.h"
#include "PyTools.h"

class CollisionsFactory
{
public:

    //! Create one collision object from the input file
    static Collisions *create( Params &params, Patch *patch, std::vector<Species *> vecSpecies, unsigned int n_collisions, bool &debye_length_required )
    {
    
        std::vector<std::string> sg1, sg2;
        std::vector<std::vector<unsigned int>> sgroup;
        double clog;
        bool intra;
        int debug_every, Z, Z0, Z1, ionization_electrons;
        std::string filename;
        std::ostringstream mystream;
        Species *s0, *s;
        
        MESSAGE( 1, "Parameters for collisions #" << n_collisions << " :" );
        
        // Read the input file by searching for the keywords "species1" and "species2"
        // which are the names of the two species that will collide
        sg1.resize( 0 );
        sg2.resize( 0 );
        PyTools::extract( "species1", sg1, "Collisions", n_collisions );
        PyTools::extract( "species2", sg2, "Collisions", n_collisions );
        
        // Obtain the lists of species numbers from the lists of species names.
        sgroup.resize( 2 );
        sgroup[0] = params.FindSpecies( vecSpecies, sg1 );
        sgroup[1] = params.FindSpecies( vecSpecies, sg2 );
        
        // Each group of species sgroup[0] and sgroup[1] must not be empty
        if( sgroup[0].size()==0 ) {
            ERROR( "In collisions #" << n_collisions << ": No valid `species1`" );
        }
        if( sgroup[1].size()==0 ) {
            ERROR( "In collisions #" << n_collisions << ": No valid `species2`" );
        }
        
        // sgroup[0] and sgroup[1] can be equal, but cannot have common species if they are not equal
        if( sgroup[0] != sgroup[1] ) {
            for( unsigned int i0=0; i0<sgroup[0].size(); i0++ ) {
                for( unsigned int i1=0; i1<sgroup[1].size(); i1++ ) {
                    if( sgroup[0][i0] == sgroup[1][i1] )
                        ERROR( "In collisions #" << n_collisions << ": species #" << sgroup[0][i0]
                               << " cannot collide with itself" );
                }
            }
            intra = false;
        } else {
            intra = true;
        }
        
        // Coulomb logarithm (if negative or unset, then automatically computed)
        clog = 0.; // default
        PyTools::extract( "coulomb_log", clog, "Collisions", n_collisions );
        if( clog <= 0. ) {
            debye_length_required = true;    // auto coulomb log requires debye length
        }
        
        // Number of timesteps between each debug output (if 0 or unset, no debug)
        debug_every = 0; // default
        PyTools::extract( "debug_every", debug_every, "Collisions", n_collisions );
        
        // Collisional ionization
        Z = 0; // default
        PyObject * ionizing = PyTools::extract_py( "ionizing", "Collisions", n_collisions );
        bool ionization = false;
        ionization_electrons = -1;
        Particles * ionization_particles = NULL;
        
        // If `ionizing` is a species name, then use that one
        std::string ionization_electrons_name = "";
        if( PyTools::convert( ionizing, ionization_electrons_name ) ) {
            
            for( int i=0; i<(int)vecSpecies.size(); i++ ) {
                if( vecSpecies[i]->name == ionization_electrons_name ) {
                    ionization_electrons = i;
                    break;
                }
            }
            if( ionization_electrons < 0 ) {
                ERROR( "In collisions #" << n_collisions << ": ionizing in unknown species `" << ionization_electrons_name << "`" );
            }
            if( vecSpecies[ionization_electrons]->atomic_number != 0 ) {
                ERROR( "In collisions #" << n_collisions << ": ionization species are not electrons (atomic_number>0)" );
            }
            ionization = true;
        
        } else if( ionizing == Py_True ) {
            
            ionization = true;
        
        } else if( ionizing != Py_False ) {
            ERROR( "In collisions #" << n_collisions << ": `ionizing` must be True, False, or the name of an electron species" );
        }
        
        if( ionization ) {
            
            if( intra ) {
                ERROR( "In collisions #" << n_collisions << ": cannot ionize with intra-collisions" );
            }
            
            for( int g=0; g<2; g++ ) { // do sgroup[0], then sgroup[1]
                s0 = vecSpecies[sgroup[g][0]]; // first species of this group
                for( unsigned int i=1; i<sgroup[g].size(); i++ ) { // loop other species of same group
                    s = vecSpecies[sgroup[g][i]]; // current species
                    if( s->mass != s0->mass )
                        ERROR( "In collisions #" << n_collisions << ": species in group `species"
                               << g+1 << "` must all have same masses for ionization" );
                               
                    if( s->atomic_number != s0->atomic_number ) {
                        if( s->atomic_number * s0->atomic_number ==0 ) {
                            ERROR( "In collisions #" << n_collisions << ": species in group `species"
                                   << g+1 << "` cannot be mixed electrons and ions for ionization" );
                        } else {
                            ERROR( "In collisions #" << n_collisions << ": species in group `species"
                                   << g+1 << "` must all have same atomic_number for ionization" );
                        }
                    }
                }
            }
            // atomic number
            Z0 = vecSpecies[sgroup[0][0]]->atomic_number;
            Z1 = vecSpecies[sgroup[1][0]]->atomic_number;
            Z = ( int )( Z0>Z1 ? Z0 : Z1 );
            if( Z0*Z1!=0 ) {
                ERROR( "In collisions #" << n_collisions << ": ionization requires electrons (no or null atomic_number)" );
            }
            if( Z==0 ) {
                ERROR( "In collisions #" << n_collisions << ": ionization requires ions (atomic_number>0)" );
            }
            // If ionizing = True, then select ionization electrons as 1st electron species
            if( ionizing == Py_True ) {
                if( Z0==0 ) {
                    ionization_electrons = sgroup[0][0];
                } else if( Z1==0 ) {
                    ionization_electrons = sgroup[1][0];
                }
            }
            ionization_particles = vecSpecies[ionization_electrons]->particles;
        }
        
        // Print collisions parameters
        mystream.str( "" ); // clear
        mystream << "(" << sgroup[0][0];
        for( unsigned int rs=1 ; rs<sgroup[0].size() ; rs++ ) {
            mystream << " " << sgroup[0][rs];
        }
        if( intra ) {
            MESSAGE( 2, "Intra collisions within species " << mystream.str() << ")" );
        } else {
            mystream << ") and (" << sgroup[1][0];
            for( unsigned int rs=1 ; rs<sgroup[1].size() ; rs++ ) {
                mystream << " " << sgroup[1][rs];
            }
            MESSAGE( 2, "Collisions between species " << mystream.str() << ")" );
        }
        MESSAGE( 2, "Coulomb logarithm: " << clog );
        if( debug_every>0 ) {
            MESSAGE( 2, "Debug every " << debug_every << " timesteps" );
        }
        mystream.str( "" ); // clear
        if( ionization_electrons>0 ) {
            MESSAGE( 2, "Collisional ionization with atomic number "<<Z<<" towards species `"<<vecSpecies[ionization_electrons]->name << "`" );
        }
        
        // If debugging log requested
        if( debug_every>0 ) {
            // Build the file name
            mystream.str( "" );
            mystream << "Collisions" << n_collisions << ".h5";
            filename = mystream.str();
            std::ifstream file( filename );
            // Check if file exists
            if( ! file ) {
                // Create the file access protocol for writing in the debug file later
                hid_t file_access = H5Pcreate( H5P_FILE_ACCESS );
                H5Pset_fapl_mpio( file_access, MPI_COMM_WORLD, MPI_INFO_NULL );
                // Create file
                hid_t fileId = H5Fcreate( filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, file_access );
                H5Pclose( file_access );
                // write all parameters as HDF5 attributes
                H5::attr( fileId, "Version", std::string( __VERSION ) );
                mystream.str( "" );
                mystream << sgroup[0][0];
                for( unsigned int i=1; i<sgroup[0].size(); i++ ) {
                    mystream << "," << sgroup[0][i];
                }
                H5::attr( fileId, "species1", mystream.str() );
                mystream.str( "" );
                mystream << sgroup[1][0];
                for( unsigned int i=1; i<sgroup[1].size(); i++ ) {
                    mystream << "," << sgroup[1][i];
                }
                H5::attr( fileId, "species2", mystream.str() );
                H5::attr( fileId, "coulomb_log", clog );
                H5::attr( fileId, "debug_every", debug_every );
                H5Fclose( fileId );
            }
        }
        
        // new Collisions object
        if( sgroup[0].size()>1 || sgroup[1].size()>1 ) {
            return new Collisions(
                       params,
                       n_collisions,
                       sgroup[0],
                       sgroup[1],
                       clog, intra,
                       debug_every,
                       Z,
                       ionization_electrons,
                       ionization_particles,
                       params.nDim_particle,
                       filename
                   );
        } else {
            return new CollisionsSingle(
                       params,
                       n_collisions,
                       sgroup[0],
                       sgroup[1],
                       clog, intra,
                       debug_every,
                       Z,
                       ionization_electrons,
                       ionization_particles,
                       params.nDim_particle,
                       filename
                   );
        }
    }
    
    
    //! Creates a vector of collisions objects
    static std::vector<Collisions *> create( Params &params, Patch *patch, std::vector<Species *> vecSpecies )
    {
        std::vector<Collisions *> vecCollisions;
        bool debye_length_required = false;
        
        // Needs reference_angular_frequency_SI to be defined
        unsigned int numcollisions=PyTools::nComponents( "Collisions" );
        if( numcollisions > 0 )
            if( params.reference_angular_frequency_SI <= 0. ) {
                ERROR( "The parameter `reference_angular_frequency_SI` needs to be defined and positive to compute collisions" );
            }
            
        // Loop over each binary collisions group and parse info
        for( unsigned int n_collisions = 0; n_collisions < numcollisions; n_collisions++ ) {
            vecCollisions.push_back( create( params, patch, vecSpecies, n_collisions, debye_length_required ) );
        }
        for( unsigned int n_collisions = 0; n_collisions < numcollisions; n_collisions++ ) {
            if( vecCollisions[ n_collisions ]->Ionization ) {
                vecCollisions[ n_collisions ]->Ionization->assignDatabase( vecCollisions[ n_collisions ]->Ionization->dataBaseIndex );
            }
        }
        
        // pass the variable "debye_length_required" into the Collision class
        Collisions::debye_length_required = debye_length_required;
        
        return vecCollisions;
    }
    
    
    //! Clone a vector of Collisions objects
    static std::vector<Collisions *> clone( std::vector<Collisions *> vecCollisions, Params &params )
    {
        std::vector<Collisions *> newVecCollisions( 0 );
        
        for( unsigned int i=0; i<vecCollisions.size(); i++ ) {
            if( dynamic_cast<CollisionsSingle *>( vecCollisions[i] ) ) {
                newVecCollisions.push_back( new CollisionsSingle( vecCollisions[i], params.nDim_particle ) );
            } else {
                newVecCollisions.push_back( new Collisions( vecCollisions[i], params.nDim_particle ) );
            }
        }
        
        return newVecCollisions;
    }
    
    
};

#endif
