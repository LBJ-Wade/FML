#include <FML/FileUtils/FileUtils.h>
#include <FML/FriendsOfFriends/FoF.h>
#include <FML/Global/Global.h>
#include <FML/MPIParticles/MPIParticles.h>
#include <FML/ParticleTypes/SimpleParticle.h>

#include <fstream>
#include <vector>

//==================================================================
// Set up a simple particle compatible with MPIParticles
//==================================================================

template <int NDIM>
struct Particle {
    double x[NDIM];
    Particle() = default;
    Particle(double * _x) { std::memcpy(x, _x, NDIM * sizeof(double)); }
    int get_ndim() { return NDIM; }
    double * get_pos() { return x; }

    // If you want the algorithm to store the FoFID in the particle add these methods
    // For particles not belonging to a group we set it equal to FML::FOF::no_FoF_ID (which is SIZE_MAX)
    // size_t FoFID;
    // void set_fofid(size_t _FoFID){ FoFID = _FoFID; }
    // size_t get_fofid(){ return FoFID; }
};

int main() {
    const int NDIM = 3;

    if (FML::ThisTask == 0)
        std::cout << "Reading particles from file\n";

    //==================================================================
    // Read ascii file with [x,y,z]
    //==================================================================
    const double box = 1024.0;
    const std::string filename = "../../../TestData/particles_B1024.txt";
    const int ncols = 3;
    const int nskip_header = 0;
    const std::vector<int> cols_to_keep{0, 1, 2};
    auto data = FML::FILEUTILS::read_regular_ascii(filename, ncols, cols_to_keep, nskip_header);

    // Create particles and scale to [0,1)
    std::vector<Particle<NDIM>> part;
    part.reserve(data.size());
    for (auto & pos : data) {
        for (auto & x : pos) {
            x /= box;
            if (x < 0.0)
                x += 1.0;
            if (x >= 1.0)
                x -= 1.0;
        }
        if (pos[0] >= FML::xmin_domain and pos[0] < FML::xmax_domain)
            part.push_back(Particle<NDIM>(pos.data()));
    }

    //==================================================================
    // Create MPI particles by letting each task keep only the particles that falls in its domain
    //==================================================================
    FML::PARTICLE::MPIParticles<Particle<NDIM>> p;
    const bool all_tasks_have_the_same_particles = false;
    const auto nalloc_per_task = not all_tasks_have_the_same_particles ? part.size() : part.size() / FML::NTasks * 2;
    p.create(part.data(),
             part.size(),
             nalloc_per_task,
             FML::xmin_domain,
             FML::xmax_domain,
             all_tasks_have_the_same_particles);
    p.info();

    //==================================================================
    // Select the class which determines what data we bin up over the particles
    // This is the fiducial one which is just position and velocity (if the particle has it)
    //==================================================================
    using FoFHalo = FML::FOF::FoFHalo<Particle<NDIM>, NDIM>;

    //==================================================================
    // Do Friend of Friend finding
    //==================================================================
    const double linking_length = 0.3;
    const double fof_distance = linking_length / std::pow(p.get_npart_total(), 1.0 / NDIM);
    const int n_min_FoF_group = 20;
    const bool periodic_box = true;

    std::vector<FoFHalo> FoFGroups;
    FML::FOF::FriendsOfFriends<Particle<NDIM>, NDIM>(
        p.get_particles_ptr(), p.get_npart(), fof_distance, n_min_FoF_group, periodic_box, FoFGroups);

    //==================================================================
    // Output (as currently written task 0 has all the halos at this point)
    // (We don't read velocities to these will just be 0)
    //==================================================================
    if (FML::ThisTask == 0) {
        std::ofstream fp("fof.txt");
        std::sort(FoFGroups.begin(), FoFGroups.end(), [&](const FoFHalo & a, const FoFHalo & b) {
            return a.pos[0] > b.pos[0];
        });
        for (auto & g : FoFGroups) {
            if (g.np > 0) {
                fp << g.np << " ";
                for (int idim = 0; idim < NDIM; idim++)
                    fp << g.pos[idim] * box << " ";
                fp << "\n";
            }
        }
        std::cout << FoFGroups.size() << "\n";
    }
}
