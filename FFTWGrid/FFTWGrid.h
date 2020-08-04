#ifndef FFTWGRIDMPI_HEADER
#define FFTWGRIDMPI_HEADER
#include <vector>
#include <complex>
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstring>
#include <functional>
#ifdef USE_FFTW
#include <fftw3.h>
#endif
#ifdef USE_MPI
#include <mpi.h>
#ifdef USE_FFTW
#include <fftw3-mpi.h>
#endif
#endif

#include <FML/Global/Global.h>

//==========================================================================
//
// Class for holding grids and performing real-to-complex and complex-to-real 
// FFTs using FFTW with MPI. Templated on dimension.
// The real and fourier grid is stored in the same array to save memory
// and all transforms are done in-place. Keeps track of the status of the grid, i.e.
// if its in real-space or fourier-space (set with set_grid_status_real)
//
// FFTWGrid<N> (Nmesh, n_extra_left, n_extra_right)
//   N                : Dimension of the grid
//   Nmesh            : Number of grid-nodes per dimension (assuming the same)
//   n_extra          : Alloc extra slices of the grid in the x-dimension (left and/or right)
//
// Compile-time defines:
//   NO_AUTO_FFTW_MPI_INIT      : Do not automatically initialize FFTW, handle this yourself
//   BOUNDSCHECK_FFTWGRID       : bound checks when setting and getting values
//   SINGLE_PRECISION_FFTW      : use float instead of double
//   LONG_DOUBLE_PRECISION_FFTW : use load double instead of double
//   DEBUG_FFTWGRID             : Show some info while running
//   USE_MPI                    : Use MPI
//   USE_OMP                    : Use OpenMP
//   USE_FFTW_THREADS           : Use threads if possible. With this we assume the maximum number of threads
//                                If you want to use fewer then you can call create_wisdom(..., nthreads) to change this
//
// External variables/methods we need from global:
//    ThisTask             : The MPI task number
//    NTasks               : Number of MPI tasks
//    using IndexIntType   = long long int;
//    using FloatType      = double;
//    using ComplexType    = std::complex<FloatType>;
//    #define assert_mpi(Expr, Msg) __assert_mpi(#Expr, Expr, __FILE__, __LINE__, Msg)
//    inline void __assert_mpi(const char* expr_str, bool expr, const char* file, int line, const char* msg);
//    inline long long int power(int base, int exponent);
//
// The grid-stucture is only compatible with in-place FFTW transforms so
// for using the methods that FT from one grid to another we copy the grid and
// transforms that
//
//==========================================================================

namespace FML {
  namespace GRID {

    // Include type definitions
#include "FFTWGlobal.h"

    // Forward declaration of range classes
    class FourierRange;
    class RealRange;

    template<int N>
      class FFTWGrid{
        private:

          // The raw data vectors. These have the format [extra slices left][main grid][extra slices right] 
          // Vector = std::vector<ComplexType> with possible custom allocator
          Vector<ComplexType> fourier_grid_raw;

          // Mesh size and the dimension of the grid
          int Nmesh;
          // Number of local slices in real and Fourier space (the same)
          ptrdiff_t Local_nx;
          // The index in the global grid the local grid starts at
          ptrdiff_t Local_x_start;

          // The total number of grid-cells we allocate
          ptrdiff_t NmeshTotComplexAlloc;
          ptrdiff_t NmeshTotRealAlloc;

          // The number of grid-cells that is active in the main part of the grid 
          ptrdiff_t NmeshTotComplex;
          ptrdiff_t NmeshTotReal;

          // XXX Add below and remove non-needed things
          // NComplexCellsBeforeMain
          // NComplexCellsBeforeRight

          // Number of extra cells per slice
          ptrdiff_t NmeshTotComplexSlice;
          ptrdiff_t NmeshTotRealSlice;

          // Number of extra slices to the left and right of the grid
          int n_extra_x_slices_left;
          int n_extra_x_slices_right;

          // If you want to keep track of the field is in real space or in Fourier space
          bool grid_is_in_real_space = true;

        public:

          // Constructors
          FFTWGrid();
          FFTWGrid(
              int Nmesh, 
              int n_extra_x_slices_left = 0, 
              int n_extra_x_slices_right = 0
              );

          // Allow copying and assignment from grids with the same dimension only
          FFTWGrid(const FFTWGrid &rhs) = default;
          FFTWGrid& operator=(const FFTWGrid &rhs) = default;

          // Pointers to various parts of the grid
          FloatType *get_real_grid_left();               // The left most slice (slice ix = -nleft_extra,...,-2,-1)
          FloatType *get_real_grid();                    // The main grid       (slice ix = 0...NLocal_x-1)
          FloatType *get_real_grid_right();              // The right grid      (slice ix = NLocal_x,NLocal_x+1,...,NLocal_x+nright_extra-1)
          FloatType *get_real_grid_by_slice(int slice);  // Get the ix'th slice (i.e. -nleft_extra <= ix < NLocal_x+nright_extra)
          ComplexType *get_fourier_grid();               // The Fourier grid (aligns with the main real grid)
#ifdef USE_FFTW
          my_fftw_complex * get_fftw_grid();             // The fftw_complex cast of the ptr above 
#endif
          // Clear the memory associated with the grid
          void free();

          // Perform real-to-complex fourier transform
          void fftw_r2c();

          // Perform complex-to-real fourier transform
          void fftw_c2r();

          // Fill the whole grid with a constant value
          void fill_real_grid(const FloatType val);
          void fill_fourier_grid(const ComplexType val);

          // Fill the main grid from a function specifying the value at a given position
          void fill_real_grid(std::function<FloatType(std::vector<double>&)> & func);
          void fill_fourier_grid(std::function<ComplexType(std::vector<double>&)> & func);

          // Get the cell coordinates from the index
          std::vector<int> get_coord_from_index(const IndexIntType index_real);
          std::vector<int> get_fourier_coord_from_index(const IndexIntType index_fourier);

          // From integer position in grid in [0,Nmesh)^Ndim to index in allocated grid
          IndexIntType get_index_real_2d(const int i, const int j) const;
          IndexIntType get_index_real_3d(const int i, const int j, const int k) const;
          IndexIntType get_index_real(const std::vector<int> &coord) const;

          // From integer position in fourier grid in [0,Nmesh)^Ndim-1 x [0,Nmesh/2+1) to index in allocated grid
          IndexIntType get_index_fourier_2d(const int i, const int j) const;
          IndexIntType get_index_fourier_3d(const int i, const int j, const int k) const;
          IndexIntType get_index_fourier(const std::vector<int> &coord) const;

          // Fetch value in grid by integer coordinate
          FloatType get_real_2d(const int i, const int j) const;
          FloatType get_real_3d(const int i, const int j, const int k) const;
          FloatType get_real(const std::vector<int> &coord) const;
          FloatType get_real_from_index(const IndexIntType index) const;

          // Fetch value in fourier grid by integer coordinate
          ComplexType get_fourier_2d(const int i, const int j) const;
          ComplexType get_fourier_3d(const int i, const int j, const int k) const;
          ComplexType get_fourier(const std::vector<int> &coord) const;
          ComplexType get_fourier_from_index(const IndexIntType index) const;

          // The position of a real grid-node in [0,1)^Ndim
          std::vector<double> get_real_position_2d(const int i, const int j) const;
          std::vector<double> get_real_position_3d(const int i, const int j, const int k) const;
          std::vector<double> get_real_position(const std::vector<int> &coord) const;

          // The  wave-vector of a grid-node in Fourier space (For physical [k] multiply by 1/Boxsize)
          std::vector<double> get_fourier_wavevector_2d(const int i, const int j) const;
          std::vector<double> get_fourier_wavevector_3d(const int i, const int j, const int k) const;
          std::vector<double> get_fourier_wavevector(const std::vector<int> &coord) const;
          std::vector<double> get_fourier_wavevector_from_index(const IndexIntType index) const;

          // Set value in grid using integer coordinate in [0,Nmesh)^Ndim
          void set_real_2d(const int i, const int j, const FloatType value);
          void set_real_3d(const int i, const int j, const int k, const FloatType value);
          void set_real(const std::vector<int> &coord, const FloatType value);
          void set_real_from_index(const IndexIntType ind, const FloatType value);
          void add_real(const std::vector<int> &coord, const FloatType value);

          // Set value in fourier grid using coordinate in [0,Nmesh)^Ndim-1 x [0,Nmesh/2+1)
          void set_fourier_2d(const int i, const int j, const ComplexType value);
          void set_fourier_3d(const int i, const int j, const int k, const ComplexType value);
          void set_fourier(const std::vector<int> &coord, const ComplexType value);
          void set_fourier_from_index(const IndexIntType ind, const ComplexType value);

          // How many extra slices we have allocated to the left
          int get_n_extra_slices_left() const;
          // How many extra slices we have allocated to the right
          int get_n_extra_slices_right() const;
          // Number of grid-nodes per dimension 
          int get_nmesh() const; 
          // Number of dimension of the grid
          int get_ndim() const;

          // Number of local x-slices in the real grid
          ptrdiff_t get_local_nx() const;
          // The index of the x-slice the first local slice is in the global grid
          ptrdiff_t get_local_x_start() const;

          // The number of active real cells (with padding)
          ptrdiff_t get_ntot_real() const;
          // Total number of active Fourier space grid-nodes [ e.g. Local_nx * N * N * ... * (N/2 + 1) ]
          ptrdiff_t get_ntot_fourier() const;
          // Total number of Fourier space grid-nodes we allocate (same as above + the extra slices)
          ptrdiff_t get_ntot_fourier_alloc() const;

          // From index in the grid get the k-vector and the norm
          void get_fourier_wavevector_and_norm_by_index(
              const IndexIntType ind, std::vector<double> &kvec, double &kmag) const;
          void get_fourier_wavevector_and_norm2_by_index(
              const IndexIntType ind, std::vector<double> &kvec, double &kmag2) const;

          // Range iterator for going through all active cells in the main real/complex grid by index
          // [ e.g. for(auto and real_index: grid.real_range()) ]
          RealRange    get_real_range();
          FourierRange get_fourier_range();

          // The number of cells per slice that we alloc. Useful to jump from slice to slice
          ptrdiff_t get_ntot_real_slice_alloc() const;

          // Check if we have NaN in any of the grids
          bool nan_in_grids() const;

          // Send extra slices to the neighboring CPUs
          void communicate_boundaries();

          // This creates wisdom (but overwrites the arrays so must be done before setting the arrays)
          void create_wisdow(int planner_flag, int use_nthreads);
          void load_wisdow(std::string filename) const;
          void save_wisdow(std::string filename) const;

          // Print some info about the grid
          void info();

          // Get/Set the status of the grid: is it currently a real grid or a fourier grid?
          bool get_grid_status_real();
          void set_grid_status_real(bool grid_is_a_real_grid);

          // For memory logging
          void add_memory_label(std::string label);

          void reallocate(int Nmesh, int nleft, int nright){
            FFTWGrid(Nmesh, nleft, nright);
          }

          // Save and read from file (adds .X to fileprefix where X is ThisTask)
          void load_from_file(std::string fileprefix);
          void dump_to_file(std::string fileprefix);

      };

    template<int N> 
      void FFTWGrid<N>::add_memory_label(std::string label){
#ifdef MEMORY_LOGGING
        FML::MemoryLog::get()->add_label(fourier_grid_raw.data(), fourier_grid_raw.capacity() * sizeof(ComplexType), label);
#endif
      }

    // Perform a real-to-complex FFT from one grid to another
    template<int N> 
      void fftw_r2c(FFTWGrid<N> &in_grid, FFTWGrid<N> &out_grid);

    // Perform a complex-to-real FFT from one grid to another
    template<int N> 
      void fftw_c2r(FFTWGrid<N> &in_grid, FFTWGrid<N> &out_grid);

    //===================================================================================
    // For range based loop over the real grid
    // For In-Place FFTW arrays there are 2 extra cells per dimension in the last dimension
    // that we need to skip when looping through all the real cells
    //===================================================================================

    // An iterator that deal with the fact that the grid has padding so we need to skip
    // some cells when looping through it
    class LoopIteratorReal {
      private:
        int index, real_index, Nmesh, odd;
      public:

        LoopIteratorReal(int _index, int _Nmesh) 
          : real_index(_index + 2*(_index/_Nmesh)), index(_index), Nmesh(_Nmesh), odd(_Nmesh % 2){}
        bool operator!=(LoopIteratorReal const &other) const{ 
          return index != other.index; 
        }
        int const & operator*() const{ 
          return real_index; 
        }
        LoopIteratorReal& operator++(){
          ++index;
          if(index % Nmesh == 0) real_index += odd ? 1 : 2;
          ++real_index;
          return *this;
        }
    };

    // Loops over all active real cells in the main grid
    class RealRange {
      private:
        const int from, to, Nmesh;
      public:
        RealRange(int _from, int _to, int _Nmesh) 
          : from(_from), to(_to), Nmesh(_Nmesh){}
        LoopIteratorReal begin() const{ 
          return { from, Nmesh }; 
        }
        LoopIteratorReal end() const{ 
          return { to,   Nmesh }; 
        }
    };

    // For range based loop over the fourier grid
    class LoopIteratorFourier {
      private:
        IndexIntType index;
      public:
        LoopIteratorFourier(IndexIntType _index) : index(_index) {}
        bool operator!=(LoopIteratorFourier const &other) const{ return index != other.index; }
        IndexIntType const & operator*() const{ 
          return index; 
        }
        LoopIteratorFourier& operator++(){
          ++index;
          return *this;
        }
    };

    class FourierRange {
      private:
        const IndexIntType from, to;
      public:
        FourierRange(IndexIntType _from, IndexIntType _to) : from(_from), to(_to){}
        LoopIteratorFourier begin() const{ 
          return { from }; 
        }
        LoopIteratorFourier end() const{ 
          return { to   }; 
        }
    };

    template<int N>
      RealRange FFTWGrid<N>::get_real_range()   { 
#ifdef DEBUG_FFTWGRID
        if(not grid_is_in_real_space){
          if(FML::ThisTask == 0) 
            std::cout << "Warning: [FFTWGrid::get_real_range] The grid status is [Fourierspace]\n";
        }
#endif
        // Here NmeshTotReal = LocalNx * Nmesh^N-1
        return RealRange(0, NmeshTotReal, Nmesh); 
      }

    template<int N>
      FourierRange FFTWGrid<N>::get_fourier_range(){ 
#ifdef DEBUG_FFTWGRID
        if(grid_is_in_real_space){
          if(FML::ThisTask == 0) 
            std::cout << "Warning: [FFTWGrid::get_fourier_range] The grid status is [Realspace]\n";
        }
#endif
        return FourierRange(0, power(Nmesh,N-2)*(Nmesh/2+1)*Local_nx);  
        //return FourierRange(0, NmeshTotComplex);  
      }

    template<int N>
      FloatType * FFTWGrid<N>::get_real_grid_left(){
        return reinterpret_cast<FloatType*>( fourier_grid_raw.data() );
      }

    template<int N>
      FloatType * FFTWGrid<N>::get_real_grid(){
        return get_real_grid_left() + NmeshTotRealSlice * n_extra_x_slices_left;
      }

    template<int N>
      FloatType * FFTWGrid<N>::get_real_grid_by_slice(int slice){
#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(-n_extra_x_slices_left <= slice and slice < Local_nx+n_extra_x_slices_right,
            "[FFTWGrid::get_real_grid] Bounds check failed\n");
#endif
        return get_real_grid_left() + NmeshTotRealSlice * (n_extra_x_slices_left + slice);
      }

    template<int N>
      FloatType * FFTWGrid<N>::get_real_grid_right(){
        return get_real_grid_left() + NmeshTotRealSlice * (n_extra_x_slices_left + Local_nx);
      }


    template<int N>
      ComplexType * FFTWGrid<N>::get_fourier_grid(){
        return fourier_grid_raw.data() + NmeshTotComplexSlice * n_extra_x_slices_left;
      }

    template<int N>
      void FFTWGrid<N>::set_grid_status_real(bool grid_is_a_real_grid){
        grid_is_in_real_space = grid_is_a_real_grid;
      }

    template<int N>
      bool FFTWGrid<N>::get_grid_status_real(){
        return grid_is_in_real_space;
      }

    template<int N>
      void FFTWGrid<N>::info(){
        if(FML::ThisTask > 0) return;
        std::string myfloattype = "[Unknown]";
        std::string status = grid_is_in_real_space ? "[Realspace]" : "[Fourierspace]";
        if(sizeof(FloatType) == sizeof(float))       myfloattype = "[Float]";
        if(sizeof(FloatType) == sizeof(double))      myfloattype = "[Double]";
        if(sizeof(FloatType) == sizeof(long double)) myfloattype = "[Long Double]";
        double memory_in_mb = NmeshTotComplexAlloc*sizeof(ComplexType)/1e6;
        std::cout << "\n========================================================\n";
        std::cout << "FFTWGrid is in " << status << " Ndim: [" << N << "] FloatType: " << myfloattype << "\n";
        std::cout << "Grid has allocated " << memory_in_mb << " MB of memory per task\n"; 
        std::cout << "Nmesh                  " << Nmesh                  << "\n";
        std::cout << "Local_nx               " << Local_nx               << "\n";
        std::cout << "n_extra_x_slices_left  " << n_extra_x_slices_left  << "\n";
        std::cout << "n_extra_x_slices_right " << n_extra_x_slices_right << "\n";
        std::cout << "NmeshTotComplexAlloc   " << NmeshTotComplexAlloc   << "\n";
        std::cout << "NmeshTotComplex        " << NmeshTotComplex        << "\n";
        std::cout << "NmeshTotComplexSlice   " << NmeshTotComplexSlice   << "\n";
        std::cout << "NmeshTotRealAlloc      " << NmeshTotRealAlloc      << "\n";
        std::cout << "NmeshTotReal           " << NmeshTotReal           << "\n";
        std::cout << "NmeshTotRealSlice      " << NmeshTotRealSlice      << "\n";
        std::cout << "========================================================\n\n";
      }

    // Make FFTW plans with FFTW_MEASURE, FFTW_PATIENT, FFTW_EXHAUSTIVE
    template<int N>
      void FFTWGrid<N>::create_wisdow(int planner_flag, int nthreads) {
#ifdef USE_FFTW
        if(planner_flag == FFTW_ESTIMATE) return;
#ifdef USE_FFTW_THREADS
        set_fftw_nthreads(nthreads);
#endif
#ifdef DEBUG_FFTWGRID
        if(FML::ThisTask == 0){
          std::cout << "[FFTWGrid::create_wisdow] Planning flag " << planner_flag << "\n";
        }
#endif

#ifdef USE_MPI
        std::vector<ptrdiff_t> NmeshPerDim(N,Nmesh);
        my_fftw_plan plan_r2c = MAKE_PLAN_R2C(N, NmeshPerDim.data(), get_real_grid(), get_fftw_grid(), MPI_COMM_WORLD, planner_flag); 
#else
        std::vector<int> NmeshPerDim(N,Nmesh);
        my_fftw_plan plan_r2c = MAKE_PLAN_R2C(N, NmeshPerDim.data(), get_real_grid(), get_fftw_grid(), planner_flag); 
#endif
        if(FML::ThisTask == 0) std::cout << "[FFTWGrid::create_wisdow] Warning this will clear data in the grids!\n";
        DESTROY_PLAN(plan_r2c);
#endif
      }

    template<int N>
      void FFTWGrid<N>::load_wisdow(std::string filename) const{
#ifdef USE_FFTW
#ifdef DEBUG_FFTWGRID
        if(FML::ThisTask == 0){
          std::cout << "[FFTWGrid::load_wisdow] Filename " << filename << "\n";
        }
#endif
        if (FML::ThisTask == 0) fftw_import_wisdom_from_filename(filename.c_str());
#ifdef USE_MPI
        fftw_mpi_broadcast_wisdom(MPI_COMM_WORLD);
#endif
      }

    template<int N>
      void FFTWGrid<N>::save_wisdow(std::string filename) const{
#ifdef USE_MPI
        fftw_mpi_gather_wisdom(MPI_COMM_WORLD);
#endif
        if (FML::ThisTask == 0) fftw_export_wisdom_to_filename(filename.c_str());
#ifdef DEBUG_FFTWGRID
        if(FML::ThisTask == 0){
          std::cout << "[FFTWGrid::save_wisdow] Filename " << filename << "\n";
        }
#endif
#endif
      }

    template<int N>
      ptrdiff_t FFTWGrid<N>::get_ntot_real_slice_alloc() const{
        return NmeshTotRealSlice;
      }

    template<int N>
      void FFTWGrid<N>::fill_real_grid(const FloatType val){
#ifdef DEBUG_FFTWGRID
        if(not grid_is_in_real_space){
          if(FML::ThisTask == 0) 
            std::cout << "Warning: [FFTWGrid::fill_real_grid] The grid status is [Fourierspace]\n";
        }
#endif
        FloatType *begin = (FloatType *) fourier_grid_raw.data();
        FloatType *end = begin + NmeshTotRealAlloc;
        std::fill(begin, end, val);
      }

    template<int N>
      void FFTWGrid<N>::fill_real_grid(std::function<FloatType(std::vector<double>&)> & func){
#ifdef DEBUG_FFTWGRID
        if(not grid_is_in_real_space){
          if(FML::ThisTask == 0) 
            std::cout << "Warning: [FFTWGrid::fill_real_grid] The grid status is [Fourierspace]\n";
        }
#endif
        for(auto&& index : get_real_range()){
          auto coord = get_coord_from_index(index);
          auto pos = get_real_position(coord);
          auto value = func(pos);
          set_real_from_index(index, value);
        }

        communicate_boundaries();
      }

    template<int N>
      void FFTWGrid<N>::fill_fourier_grid(const ComplexType val){
#ifdef DEBUG_FFTWGRID
        if(grid_is_in_real_space){
          if(FML::ThisTask == 0) 
            std::cout << "Warning: [FFTWGrid::fill_real_grid] The grid status is [Realspace]\n";
        }
#endif
        std::fill(fourier_grid_raw.begin(), fourier_grid_raw.end(), val);
      }

    template<int N>
      void FFTWGrid<N>::fill_fourier_grid(std::function<ComplexType(std::vector<double>&)> & func){
#ifdef DEBUG_FFTWGRID
        if(grid_is_in_real_space){
          if(FML::ThisTask == 0) 
            std::cout << "Warning: [FFTWGrid::fill_real_grid] The grid status is [Realspace]\n";
        }
#endif
        for(auto&& index : get_fourier_range()){
          auto kvec = get_fourier_wavevector_from_index(index);
          auto value = func(kvec);
          set_fourier_from_index(index, value);
        }
      }

    // We copy over slices
    template<int N>
      void FFTWGrid<N>::communicate_boundaries(){
        int n_to_recv_right = n_extra_x_slices_right;
        int n_to_recv_left  = n_extra_x_slices_left;
        if(n_to_recv_right > Local_nx) n_to_recv_right  = Local_nx;
        if(n_to_recv_left  > Local_nx) n_to_recv_left = Local_nx;

#ifdef DEBUG_FFTWGRID
        if(FML::ThisTask == 0){
          std::cout << "[FFTWGrid::communicate_boundaries] Recieving " << n_to_recv_right << " from the right and " << n_to_recv_left << " slices from the left\n";
        }
#endif

        int rightcpu = (FML::ThisTask + 1         ) % FML::NTasks;
        int leftcpu  = (FML::ThisTask - 1 + FML::NTasks) % FML::NTasks;
        int bytes_slice = NmeshTotRealSlice * sizeof(FloatType);

        for(int i = 0; i < n_to_recv_right; i++){
          FloatType *slice_left_tosend  = get_real_grid() + NmeshTotRealSlice * (i);
          FloatType *slice_right_torecv = get_real_grid_right() + NmeshTotRealSlice * (i);
          char *sendbuf = reinterpret_cast<char*> (slice_left_tosend);
          char *recvbuf = reinterpret_cast<char*> (slice_right_torecv);
#ifdef USE_MPI
          MPI_Status status;
          MPI_Sendrecv(
              sendbuf, bytes_slice, MPI_CHAR, leftcpu,  0, 
              recvbuf, bytes_slice, MPI_CHAR, rightcpu, 0, 
              MPI_COMM_WORLD, &status);
#else
          std::memcpy(recvbuf, sendbuf, bytes_slice);
#endif
        }

        for(int i = 0; i < n_to_recv_left; i++){
          FloatType *slice_right_tosend = get_real_grid() + NmeshTotRealSlice * (Local_nx - 1 - i);
          FloatType *slice_left_torecv  = get_real_grid_left() + NmeshTotRealSlice * (n_extra_x_slices_left - 1 - i);
          char *sendbuf = reinterpret_cast<char*> (slice_right_tosend);
          char *recvbuf = reinterpret_cast<char*> (slice_left_torecv);

#ifdef USE_MPI
          MPI_Status status;
          MPI_Sendrecv(
              sendbuf, bytes_slice, MPI_CHAR, rightcpu, 0, 
              recvbuf, bytes_slice, MPI_CHAR, leftcpu,  0, 
              MPI_COMM_WORLD, &status);
#else
          std::memcpy(recvbuf, sendbuf, bytes_slice);
#endif
        }
      }

    template<int N>
      FFTWGrid<N>::FFTWGrid() : 
        Nmesh(0), 
        Local_nx(0),
        Local_x_start(0),
        NmeshTotRealAlloc(0), 
        NmeshTotComplexAlloc(0), 
        NmeshTotReal(0),      
        NmeshTotComplex(0), 
        NmeshTotRealSlice(0), 
        NmeshTotComplexSlice(0), 
        n_extra_x_slices_left(0), 
        n_extra_x_slices_right(0)
    {
    }

    template<int N>
      FFTWGrid<N>::FFTWGrid(int Nmesh, int n_extra_x_slices_left, int n_extra_x_slices_right) : 
        Nmesh(Nmesh), 
        Local_nx(Nmesh),
        Local_x_start(0),
        NmeshTotRealAlloc(0), 
        NmeshTotComplexAlloc(0), 
        NmeshTotReal(0),      
        NmeshTotComplex(0), 
        NmeshTotRealSlice(0), 
        NmeshTotComplexSlice(0), 
        n_extra_x_slices_left(n_extra_x_slices_left), 
        n_extra_x_slices_right(n_extra_x_slices_right)
    {

#ifdef USE_MPI
      // FFTW with MPI not implemented in FFTW for Ndim = 1 so abort
      assert_mpi(N > 1, 
          "[FFTWGrid] FFTW r2c and c2r with MPI currently not supported for 1D\n");
      assert_mpi(Nmesh % FML::NTasks == 0, 
          "[FFTWGrid] The number of CPUs should divide the gridsize. Otherwise there might be issues with extra padding in FFTW and in they way we divide the domain\n");
#endif

      // Total number of complex gridcells in the local grid
      std::vector<ptrdiff_t> NmeshPerDimFourier(N,Nmesh);
      NmeshPerDimFourier[N-1] = Nmesh/2+1;
#ifdef USE_MPI
#ifdef USE_FFTW
      NmeshTotComplex = MPI_FFTW_LOCAL_SIZE(N, NmeshPerDimFourier.data(), MPI_COMM_WORLD, &Local_nx, &Local_x_start);
#else
      // If we don't have FFTW, but want to use this class
      Local_nx = Nmesh / FML::NTasks;
      Local_x_start = FML::ThisTask * Local_nx;
      NmeshTotComplex = (Nmesh/2+1) * FML::power(Nmesh, N-1) / FML::NTasks;
#endif
#else
      NmeshTotComplex = 1;
      for(int i = 0; i < N; i++)
        NmeshTotComplex *= NmeshPerDimFourier[i];
#endif

      // Total number of real gridcells in the local grid. 
      // NB: This will differ from the number of real cells we allocate!
      NmeshTotReal = Local_nx * power(ptrdiff_t(Nmesh), N - 1);
      // Number of cells for one extra slice
      NmeshTotComplexSlice = (Nmesh/2+1) * power(ptrdiff_t(Nmesh),N-2);
      NmeshTotRealSlice    = 2 * NmeshTotComplexSlice;
      // Total number of grid-nodes we allocate including extra slices
      NmeshTotComplexAlloc = NmeshTotComplex + NmeshTotComplexSlice * (n_extra_x_slices_left + n_extra_x_slices_right);
      NmeshTotRealAlloc    = 2 * NmeshTotComplexAlloc;

      // Allocate memory and initialize to 0
      fourier_grid_raw.resize(NmeshTotComplexAlloc);
      add_memory_label("FFTWGrid");
      std::fill(fourier_grid_raw.begin(), fourier_grid_raw.end(), 0.0);

      // Check alignment (relevant for SIMD instructions) since we don't use fftw_malloc
      // If you have SIMD and the alignment is off then changing to an allignment allocator
      // will help speed up the transforms
#ifdef USE_FFTW
      if(((unsigned long) get_fftw_grid() & (16 - 1))){
        std::cout << "Warning: FFTWGrid is not 16 byte aligned (only relevant for speed if you have SIMD)\n";
      }
#endif

#ifdef DEBUG_FFTWGRID
      if(FML::ThisTask == 0){
        std::cout << "[FFTWGrid] Creating grid Nmesh = " << Nmesh << " Local_nx = " << Local_nx << 
          " n_extra: (" << n_extra_x_slices_left << " + " << n_extra_x_slices_right << ")\n";
      }
#endif
    }

    template<int N>
      IndexIntType FFTWGrid<N>::get_index_real_2d(const int i, const int j) const{
#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(-n_extra_x_slices_left <= i and i < Local_nx+n_extra_x_slices_right and 0 <= j and j < Nmesh and N == 2, 
            "[FFTWGrid::get_index_real_2d] Bounds check failed\n");
#endif
        return i * 2 * (Nmesh/2+1) + j;
      }

    template<int N>
      IndexIntType FFTWGrid<N>::get_index_real_3d(const int i, const int j, const int k) const{
#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(-n_extra_x_slices_left <= i and i < Local_nx+n_extra_x_slices_right and 0 <= j and j < Nmesh and 0 <= k and k < Nmesh and N == 3, 
            "[FFTWGrid::get_index_real_3d] Bounds check failed\n");
#endif
        return (Nmesh * i + j)*2*(Nmesh/2+1) + k;
      }

    template<int N>
      std::vector<int> FFTWGrid<N>::get_coord_from_index(const IndexIntType index_real){
#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(index_real >= -NmeshTotRealSlice * n_extra_x_slices_left and index_real < NmeshTotRealSlice * (Local_nx + n_extra_x_slices_right),
            "[FFTWGrid::get_coord_from_index] Bounds check failed\n");
#endif
        std::vector<int> coord(N);
        int nmesh_plus_padding = (2 * (Nmesh / 2 + 1));
        IndexIntType index = index_real;
        coord[N-1] = index % nmesh_plus_padding;
        index /= nmesh_plus_padding;

        if(N == 2){
          coord[0] = index;
        } else if (N == 3){
          coord[1] = index % Nmesh;
          coord[0] = index / Nmesh;
        } else {
          for(int idim = N-2; idim >=1; idim--){
            coord[idim] = index % Nmesh;
            index /= Nmesh;
          }
          coord[0] = index;
        }
        return coord;
      }

    template<int N>
      IndexIntType FFTWGrid<N>::get_index_real(const std::vector<int> &coord) const{
#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(coord.size() == N, 
            "[FFTWGrid::get_index_real] Coord has wrong size\n");
#endif
        if(N == 2) return get_index_real_2d(coord[0],coord[1]);
        if(N == 3) return get_index_real_3d(coord[0],coord[1],coord[2]);

#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(-n_extra_x_slices_left <= coord[0] and coord[0] < Local_nx+n_extra_x_slices_right,
            "[FFTWGrid::get_index_real] Bounds check failed (first coordinate)\n");
        for(int idim = 1; idim < N; idim++){
          assert_mpi(0 <= coord[idim] and coord[idim] < Nmesh, 
              "[FFTWGrid::get_index_real] Bounds check failed\n");
        }
#endif

        IndexIntType index = coord[0];
        for(int idim = 1; idim < N-1; idim++){
          index = index * Nmesh + coord[idim];
        }
        index = index * (2 * (Nmesh / 2 + 1)) + coord[N-1];
        return index;
      }

    template<int N>
      IndexIntType FFTWGrid<N>::get_index_fourier_2d(const int i, const int j) const{
#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(0 <= i and i < Local_nx and 0 <= j and j < Nmesh/2+1 and N == 2, 
            "[FFTWGrid::get_index_fourier_2d] Bounds check failed\n");
#endif
        return i*(Nmesh/2+1) + j;
      }

    template<int N>
      IndexIntType FFTWGrid<N>::get_index_fourier_3d(const int i, const int j, const int k) const{
#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(0 <= i and i < Local_nx and 0 <= j and j < Nmesh and 0 <= k and k < Nmesh/2+1 and N == 3, 
            "[FFTWGrid::get_index_fourier_3d] Bounds check failed\n");
#endif
        return (Nmesh * i + j)*(Nmesh/2+1) + k;
      }

    template<int N>
      IndexIntType FFTWGrid<N>::get_index_fourier(const std::vector<int> &coord) const{
        if(N == 2) return get_index_fourier_2d(coord[0],coord[1]);
        if(N == 3) return get_index_fourier_3d(coord[0],coord[1],coord[2]);

#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(0 <= coord[0] and coord[0] < Local_nx, 
            "[FFTWGrid::get_index_fourier] Bounds check failed\n");
        for(int idim = 1; idim < N; idim++){
          assert_mpi(0 <= coord[idim] and coord[idim] < Nmesh, 
              "[FFTWGrid::get_index_fourier] Bounds check failed\n");
        }
#endif

        IndexIntType index = coord[0];
        for(int idim = 1; idim < N-1; idim++){
          index = index * Nmesh + coord[idim];
        }
        index = index * (Nmesh / 2 + 1) + coord[N-1];
        return index;
      }

    template<int N>
      void FFTWGrid<N>::fftw_r2c(){
#ifdef USE_FFTW

#ifdef DEBUG_FFTWGRID
        if(FML::ThisTask == 0){
          std::cout << "[FFTWGrid::fftw_r2c] Transforming grid to fourier space\n";
        }
        if(not grid_is_in_real_space){
          if(FML::ThisTask == 0) 
            std::cout << "Warning: [FFTWGrid::fftw_r2c] Transforming grid whose status is already [Fourierspace]\n";
        }
#endif

        //=================================================================================
        // Make a copy of the first few cells that might be overwritten when doing the FFT
        // This might mess up extra right slices we have if we don't make a copy here
        //=================================================================================
        FloatType *real_grid_right = get_real_grid_right();
        std::vector<FloatType> right_copy;
        if(n_extra_x_slices_right > 0){
          right_copy = std::vector<FloatType>(Nmesh/2+1);
          for(int i = 0; i < Nmesh/2+1; i++) right_copy[i] = real_grid_right[i];
        }

#ifdef USE_MPI
        std::vector<ptrdiff_t> NmeshPerDim(N,Nmesh);
        my_fftw_plan plan_r2c = MAKE_PLAN_R2C(N, NmeshPerDim.data(), get_real_grid(), get_fftw_grid(), MPI_COMM_WORLD, FFTW_ESTIMATE); 
#else
        std::vector<int> NmeshPerDim(N,Nmesh);
        my_fftw_plan plan_r2c = MAKE_PLAN_R2C(N, NmeshPerDim.data(), get_real_grid(), get_fftw_grid(), FFTW_ESTIMATE); 
#endif

        EXECUTE_FFT(plan_r2c);
        grid_is_in_real_space = false;

        // Normalize
        double norm = 1.0 / std::pow(double(Nmesh), N);
        auto *fourier_grid = get_fourier_grid();
        for(auto && index: get_fourier_range()){
          fourier_grid[index] *= norm;  
        }

        //=================================================================================
        // Copy back data we copied
        //=================================================================================
        if(n_extra_x_slices_right > 0){
          for(int i = 0; i < Nmesh/2+1; i++) real_grid_right[i] = right_copy[i];
        }

        DESTROY_PLAN(plan_r2c);
#else
        assert_mpi(false,
            "[FFTWGrid::fftw_r2c] Compiled without FFTW support so cannot take Fourier transforms\n");
#endif
      }

    template<int N>
      void FFTWGrid<N>::fftw_c2r(){
#ifdef USE_FFTW

#ifdef DEBUG_FFTWGRID
        if(FML::ThisTask == 0){
          std::cout << "[FFTWGrid::fftw_c2r] Transforming grid to real space\n";
        }
        if(grid_is_in_real_space){
          if(FML::ThisTask == 0) 
            std::cout << "Warning: [FFTWGrid::fftw_c2r] Transforming grid whose status is already [Realspace]\n";
        }
#endif

        //=================================================================================
        // Make a copy of the first few cells that might be overwritten when doing the FFT
        // This might mess up extra right slices we have if we don't make a copy here
        //=================================================================================
        FloatType *real_grid_right = get_real_grid_right();
        std::vector<FloatType> right_copy;
        if(n_extra_x_slices_right > 0){
          right_copy = std::vector<FloatType>(Nmesh/2+1);
          for(int i = 0; i < Nmesh/2+1; i++) right_copy[i] = real_grid_right[i];
        }

#ifdef USE_MPI
        std::vector<ptrdiff_t> NmeshPerDim(N,Nmesh);
        my_fftw_plan plan_c2r = MAKE_PLAN_C2R(N, NmeshPerDim.data(), get_fftw_grid(), get_real_grid(), MPI_COMM_WORLD, FFTW_ESTIMATE); 
#else
        std::vector<int> NmeshPerDim(N,Nmesh);
        my_fftw_plan plan_c2r = MAKE_PLAN_C2R(N, NmeshPerDim.data(), get_fftw_grid(), get_real_grid(), FFTW_ESTIMATE); 
#endif

        EXECUTE_FFT(plan_c2r);  
        grid_is_in_real_space = true;

        //=================================================================================
        // Copy back data we copied
        //=================================================================================
        if(n_extra_x_slices_right > 0){
          for(int i = 0; i < Nmesh/2+1; i++) real_grid_right[i] = right_copy[i];
        }

        DESTROY_PLAN(plan_c2r);
#else
        assert_mpi(false,
            "[FFTWGrid::fftw_c2r] Compiled without FFTW support so cannot take Fourier transforms\n");
#endif
      }

    template<int N>
      FloatType FFTWGrid<N>::get_real_2d(const int i, const int j) const{
        IndexIntType index = get_index_real_2d(i,j);
        const FloatType *grid = reinterpret_cast<const FloatType*>( fourier_grid_raw.data() ) + NmeshTotRealSlice * n_extra_x_slices_left;
        return grid[index];
      }

    template<int N>
      FloatType FFTWGrid<N>::get_real_3d(const int i, const int j, const int k) const{
        IndexIntType index = get_index_real_3d(i,j,k);
        const FloatType *grid = reinterpret_cast<const FloatType*>( fourier_grid_raw.data() ) + NmeshTotRealSlice * n_extra_x_slices_left;
        return grid[index];
      }

    template<int N>
      FloatType FFTWGrid<N>::get_real(const std::vector<int> &coord) const{
        IndexIntType index = get_index_real(coord);
        const FloatType *grid = reinterpret_cast<const FloatType*>( fourier_grid_raw.data() ) + NmeshTotRealSlice * n_extra_x_slices_left;
        return grid[index];
      }

    template<int N>
      void FFTWGrid<N>::set_real_3d(const int i, const int j, const int k, const FloatType value){
        IndexIntType index = get_index_real_3d(i,j,k);
        get_real_grid()[index] = value;
      }

    template<int N>
      void FFTWGrid<N>::set_real_2d(const int i, const int j, const FloatType value){
        IndexIntType index = get_index_real_2d(i,j);
        get_real_grid()[index] = value;
      }

    template<int N>
      void FFTWGrid<N>::set_real(const std::vector<int> &coord, const FloatType value){
        IndexIntType index = get_index_real(coord);
        get_real_grid()[index] = value;
      }

    template<int N>
      void FFTWGrid<N>::add_real(const std::vector<int> &coord, const FloatType value){
        IndexIntType index = get_index_real(coord);
        get_real_grid()[index] += value;
      }

    template<int N>
      void FFTWGrid<N>::set_real_from_index(const IndexIntType index, const FloatType value){
        get_real_grid()[index] = value;
      }

    template<int N>
      ComplexType FFTWGrid<N>::get_fourier_2d(const int i, const int j) const{
        IndexIntType index = get_index_fourier_2d(i,j);
        return fourier_grid_raw[NmeshTotComplexSlice * n_extra_x_slices_left + index];
      }

    template<int N>
      ComplexType FFTWGrid<N>::get_fourier_3d(const int i, const int j, const int k) const{
        IndexIntType index = get_index_fourier_3d(i,j,k);
        return fourier_grid_raw[NmeshTotComplexSlice * n_extra_x_slices_left + index];
      }

    template<int N>
      ComplexType FFTWGrid<N>::get_fourier(const std::vector<int> &coord) const{
        IndexIntType index = get_index_fourier(coord);
        return fourier_grid_raw[NmeshTotComplexSlice * n_extra_x_slices_left + index];
      }

    template<int N>
      ComplexType FFTWGrid<N>::get_fourier_from_index(const IndexIntType index) const{
        return fourier_grid_raw[NmeshTotComplexSlice * n_extra_x_slices_left + index];
      }

    template<int N>
      void FFTWGrid<N>::set_fourier_from_index(const IndexIntType index, const ComplexType value){
        get_fourier_grid()[index] = value;
      }

    template<int N>
      void FFTWGrid<N>::set_fourier(const std::vector<int> &coord, const ComplexType value){
        IndexIntType index = get_index_fourier(coord);
        get_fourier_grid()[index] = value;
      }

    template<int N>
      void FFTWGrid<N>::set_fourier_3d(const int i, const int j, const int k, const ComplexType value){
        IndexIntType index = get_index_fourier_3d(i,j,k);
        get_fourier_grid()[index] = value;
      }

    template<int N>
      void FFTWGrid<N>::set_fourier_2d(const int i, const int j, const ComplexType value){
        IndexIntType index = get_index_fourier_2d(i,j);
        get_fourier_grid()[index] = value;
      }

#ifdef USE_FFTW
    template<int N>
      my_fftw_complex * FFTWGrid<N>::get_fftw_grid(){
        return reinterpret_cast<my_fftw_complex *>( get_fourier_grid() );
      }
#endif

    template<int N>
      int FFTWGrid<N>::get_nmesh() const{ 
        return Nmesh;
      }

    template<int N>
      int FFTWGrid<N>::get_ndim() const{
        return N;
      }

    template<int N>
      ptrdiff_t FFTWGrid<N>::get_ntot_real() const{
        return NmeshTotRealSlice * Local_nx;
      }

    template<int N>
      ptrdiff_t FFTWGrid<N>::get_ntot_fourier() const{
        return NmeshTotComplexSlice * Local_nx;
      }

    template<int N>
      ptrdiff_t FFTWGrid<N>::get_ntot_fourier_alloc() const{
        return NmeshTotComplexAlloc;
      }

    template<int N>
      ptrdiff_t FFTWGrid<N>::get_local_nx() const{
        return Local_nx;
      }

    template<int N>
      ptrdiff_t FFTWGrid<N>::get_local_x_start() const{
        return Local_x_start;
      }

    template<int N>
      std::vector<double> FFTWGrid<N>::get_real_position_2d(const int i, const int j) const{
        std::vector<double> xcoord(2);
        xcoord[0] = (Local_x_start+i)/double(Nmesh);
        xcoord[1] = j/double(Nmesh);
        return xcoord;
      }

    template<int N>
      std::vector<double> FFTWGrid<N>::get_real_position_3d(const int i, const int j, const int k) const{
        std::vector<double> xcoord(3);
        xcoord[0] = (Local_x_start+i)/double(Nmesh);
        xcoord[1] = j/double(Nmesh);
        xcoord[2] = k/double(Nmesh);
        return xcoord;
      }

    template<int N>
      std::vector<double> FFTWGrid<N>::get_real_position(const std::vector<int> &coord) const{
        std::vector<double> xcoord(N);
#ifdef CELLCENTERSHIFTED
        const constexpr double shift = 0.5;
#else
        const constexpr double shift = 0.0;
#endif
        xcoord[0] = (Local_x_start+coord[0]+shift)/double(Nmesh);
        for(int idim = 1; idim < N; idim++)
          xcoord[idim] = (coord[idim]+shift)/double(Nmesh); 
        return xcoord;
      }

    template<int N>
      void FFTWGrid<N>::get_fourier_wavevector_and_norm_by_index(const IndexIntType index, std::vector<double> &kvec, double &kmag) const{
        get_fourier_wavevector_and_norm2_by_index(index, kvec, kmag);
        kmag = std::sqrt(kmag);
      }
    
    template<int N>
      std::vector<int> FFTWGrid<N>::get_fourier_coord_from_index(const IndexIntType index) {
        const int nover2plus1 = Nmesh/2+1;
        const int nover2 = Nmesh/2;
        std::vector<int> coord(N);
        coord[N-1] = index % nover2plus1;
        for(int idim = N-2, n = nover2plus1; idim >=0; idim--, n *= Nmesh){
          coord[idim] = (index / n) % Nmesh;
        }
        return coord;
      }

    template<int N>
      void FFTWGrid<N>::get_fourier_wavevector_and_norm2_by_index(const IndexIntType index, std::vector<double> &kvec, double &kmag2) const{
        const double twopi = 2.0 * M_PI;
        const int nover2plus1 = Nmesh/2+1;
        const int nover2 = Nmesh/2;

#ifdef BOUNDSCHECK_FFTWGRID
        assert_mpi(kvec.size() == N, 
            "[FFTWGrid::get_fourier_wavevector_and_norm2_by_index] kvec has the wrong size\n");
#endif

        if(N == 3){
          int iz = index % nover2plus1;;
          int iy = index / nover2plus1;
          int ix = iy / Nmesh;
          iy = iy % Nmesh;
          kvec[0] = twopi * ((Local_x_start+ix) <= nover2 ? (Local_x_start+ix) : (Local_x_start+ix) - Nmesh);
          kvec[1] = twopi * (iy <= nover2 ? iy : iy - Nmesh);
          kvec[2] = twopi * (iz <= nover2 ? iz : iz - Nmesh);
          kmag2 = kvec[0]*kvec[0] + kvec[1]*kvec[1] + kvec[2]*kvec[2];
        } else if (N == 2){
          int iy = index % nover2plus1;
          int ix = index / nover2plus1;
          kvec[0] = twopi * ((Local_x_start+ix) <= nover2 ? (Local_x_start+ix) : (Local_x_start+ix) - Nmesh);
          kvec[1] = twopi * (iy <= nover2 ? iy : iy - Nmesh);
          kmag2 = kvec[0]*kvec[0] + kvec[1]*kvec[1];
        } else if (N == 1) {
          int ix = index;
          kvec[0] = twopi * ((Local_x_start+ix) <= nover2 ? (Local_x_start+ix) : (Local_x_start+ix) - Nmesh);
          kmag2 = kvec[0]*kvec[0];
        } else {
          int coord[N];
          coord[N-1] = index % nover2plus1;
          for(int idim = N-2, n = nover2plus1; idim >=0; idim--, n *= Nmesh){
            coord[idim] = (index / n) % Nmesh;
          }

          kvec[0] = twopi * ((Local_x_start+coord[0]) <= nover2 ? (Local_x_start+coord[0]) : (Local_x_start+coord[0]) - Nmesh);
          kmag2 = kvec[0]*kvec[0];
          for(int idim = 1; idim < N; idim++){
            kvec[idim] = twopi * (coord[idim] <= nover2 ? coord[idim] : coord[idim] - Nmesh);
            kmag2 += kvec[idim]*kvec[idim];
          }
        }
      }

    template<int N>
      std::vector<double> FFTWGrid<N>::get_fourier_wavevector_2d(const int i, const int j) const{
        const double twopi = 2.0 * M_PI;
        std::vector<double> fcoord(2);
        fcoord[0] = twopi * ( (Local_x_start+i) <= Nmesh/2 ? (Local_x_start+i) : (Local_x_start+i) - Nmesh);
        fcoord[1] = twopi * ( j <= Nmesh/2 ? j : j - Nmesh);
        return fcoord;
      }

    template<int N>
      std::vector<double> FFTWGrid<N>::get_fourier_wavevector_3d(const int i, const int j, const int k) const{
        const double twopi = 2.0 * M_PI;
        std::vector<double> fcoord(3);
        fcoord[0] = twopi * ((Local_x_start+i) <= Nmesh/2 ? (Local_x_start+i) : (Local_x_start+i) - Nmesh);
        fcoord[1] = twopi * (j <= Nmesh/2 ? j : j - Nmesh);
        fcoord[2] = twopi * (k <= Nmesh/2 ? k : k - Nmesh);
        return fcoord;
      }

    template<int N>
      std::vector<double> FFTWGrid<N>::get_fourier_wavevector(const std::vector<int> &coord) const{
        const double twopi = 2.0 * M_PI;
        std::vector<double> fcoord(N);
        fcoord[0] = twopi * ((Local_x_start+coord[0]) <= Nmesh/2 ? (Local_x_start+coord[0]) : (Local_x_start+coord[0]) - Nmesh);
        for(int idim = 1; idim < N; idim++)
          fcoord[idim] = twopi * (coord[idim] <= Nmesh/2 ? coord[idim] : coord[idim] - Nmesh);
        return fcoord;
      }

    template<int N>
      std::vector<double> FFTWGrid<N>::get_fourier_wavevector_from_index(const IndexIntType index) const{
        const double twopi = 2.0 * M_PI;
        const int nover2plus1 = Nmesh/2+1;
        const int nover2 = Nmesh/2;
        std::vector<double> fcoord(N);

        // XXX < or <= ?

        if(N == 3){
          int iz = index % nover2plus1;;
          int iy = index / nover2plus1;
          int ix = iy / Nmesh;
          iy = iy % Nmesh;
          fcoord[0] = twopi * ((Local_x_start+ix) <= nover2 ? (Local_x_start+ix) : (Local_x_start+ix) - Nmesh);
          fcoord[1] = twopi * (iy <= nover2 ? iy : iy - Nmesh);
          fcoord[2] = twopi * (iz <= nover2 ? iz : iz - Nmesh);
        } else if(N == 2){
          int iy = index % nover2plus1;
          int ix = index / nover2plus1;
          fcoord[0] = twopi * ((Local_x_start+ix) <= nover2 ? (Local_x_start+ix) : (Local_x_start+ix) - Nmesh);
          fcoord[1] = twopi * (iy <= nover2 ? iy : iy - Nmesh);
        } else if(N == 1){
          int ix = index % nover2plus1;
          fcoord[0] = twopi * ((Local_x_start+ix) <= nover2 ? (Local_x_start+ix) : (Local_x_start+ix) - Nmesh);
        } else {
          fcoord[N-1] = index % nover2plus1;
          for(int idim = N-2, n = nover2plus1; idim >=0; idim--, n *= Nmesh){
            fcoord[idim] = (index / n) % Nmesh;
          }
          fcoord[0] = twopi * ((Local_x_start+fcoord[0]) <= nover2 ? (Local_x_start+fcoord[0]) : (Local_x_start+fcoord[0]) - Nmesh);
          for(int idim = 1; idim < N; idim++){
            fcoord[idim] = twopi * (fcoord[idim] <= nover2 ? fcoord[idim] : fcoord[idim] - Nmesh);
          }
        }
        return fcoord;
      }

    template<int N>
      void fftw_c2r(FFTWGrid<N> &in_grid, FFTWGrid<N> &out_grid){
#ifdef DEBUG_FFTWGRID
        if(FML::ThisTask == 0){
          std::cout << "[fftw_c2r] Transforming grid to real space\n";
        }
#endif
        out_grid = in_grid;
        out_grid.fftw_c2r();
      }

    template<int N>
      void fftw_r2c(FFTWGrid<N> &in_grid, FFTWGrid<N> &out_grid){
#ifdef DEBUG_FFTWGRID
        if(FML::ThisTask == 0){
          std::cout << "[fftw_r2c] Transforming grid to real space\n";
        }
#endif
        out_grid = in_grid;
        out_grid.fftw_r2c();
      }

    template<int N>
      void FFTWGrid<N>::free(){
        fourier_grid_raw.clear();
        fourier_grid_raw.shrink_to_fit();
      }

    template<int N>
      FloatType FFTWGrid<N>::get_real_from_index(const IndexIntType index) const {
        const FloatType *grid = reinterpret_cast<const FloatType*>( fourier_grid_raw.data() ) + NmeshTotRealSlice * n_extra_x_slices_left;
        return grid[index];
      }

    template<int N>
      int FFTWGrid<N>::get_n_extra_slices_left() const{
        return n_extra_x_slices_left;
      }

    template<int N>
      int FFTWGrid<N>::get_n_extra_slices_right() const{
        return n_extra_x_slices_right;
      }

    template<int N>
      bool FFTWGrid<N>::nan_in_grids() const{
        bool found = false;
        for(int i = 0; i < NmeshTotComplexAlloc; i++){
          if(fourier_grid_raw[i] != fourier_grid_raw[i]){
            std::cout << "[FFTWGrid::nan_in_grids] Found NaN in grid. Index = " << i << "\n";
            found = true;
            break;
          }
        }
        return found;
      }

    template<int N>
      void FFTWGrid<N>::dump_to_file(std::string fileprefix){
        std::ios_base::sync_with_stdio(false);
        std::string filename = fileprefix + "." + std::to_string(FML::ThisTask);
        auto myfile = std::fstream(filename, std::ios::out | std::ios::binary);
        
        // If we fail to write give a warning, but continue
        if(!myfile.good()) {
          std::string error = "[FFTWGrid::dump_to_file] Failed to save the grid data on task " 
            + std::to_string(FML::ThisTask) + " Filename: " + filename;
          std::cout << error << "\n";
          return;
        }

        // Write header data
        int NDIM = N;
        myfile.write((char*)&NDIM,                   sizeof(NDIM));
        myfile.write((char*)&Nmesh,                  sizeof(Nmesh));
        myfile.write((char*)&n_extra_x_slices_left,  sizeof(n_extra_x_slices_left));
        myfile.write((char*)&n_extra_x_slices_right, sizeof(n_extra_x_slices_right));
        myfile.write((char*)&Local_nx,               sizeof(Local_nx));
        myfile.write((char*)&Local_x_start,          sizeof(Local_x_start));
        myfile.write((char*)&NmeshTotComplexAlloc,   sizeof(NmeshTotComplexAlloc));
        myfile.write((char*)&NmeshTotRealAlloc,      sizeof(NmeshTotRealAlloc));
        myfile.write((char*)&NmeshTotComplex,        sizeof(NmeshTotComplex));
        myfile.write((char*)&NmeshTotComplexSlice,   sizeof(NmeshTotComplexSlice));
        myfile.write((char*)&NmeshTotRealSlice,      sizeof(NmeshTotRealSlice));
        myfile.write((char*)&grid_is_in_real_space,  sizeof(grid_is_in_real_space));

        // Write main grid
        size_t bytes = sizeof(ComplexType) * NmeshTotComplexAlloc;
        myfile.write((char*)fourier_grid_raw.data(), bytes);

        myfile.close();
      }

    template<int N>
      void FFTWGrid<N>::load_from_file(std::string fileprefix){
        std::ios_base::sync_with_stdio(false);
        std::string filename = fileprefix + "." + std::to_string(FML::ThisTask);
        auto myfile = std::ifstream(filename, std::ios::binary);
        
        // If we fail to load a file throw an error
        if(!myfile.good()) {
          std::string error = "[FFTWGrid::load_from_file] Failed to read the grid from file on task " 
            + std::to_string(FML::ThisTask) + " Filename: " + filename;
          assert_mpi(false, error.c_str());
        }

        // Write header data
        int NDIM;
        myfile.read((char*)&NDIM,                   sizeof(NDIM));
        assert_mpi(N == NDIM, 
            "[FFTWGrid::load_from_file] The dimension of the grid does not match what is in the file");
        myfile.read((char*)&Nmesh,                  sizeof(Nmesh));
        myfile.read((char*)&n_extra_x_slices_left,  sizeof(n_extra_x_slices_left));
        myfile.read((char*)&n_extra_x_slices_right, sizeof(n_extra_x_slices_right));
        myfile.read((char*)&Local_nx,               sizeof(Local_nx));
        myfile.read((char*)&Local_x_start,          sizeof(Local_x_start));
        myfile.read((char*)&NmeshTotComplexAlloc,   sizeof(NmeshTotComplexAlloc));
        myfile.read((char*)&NmeshTotRealAlloc,      sizeof(NmeshTotRealAlloc));
        myfile.read((char*)&NmeshTotComplex,        sizeof(NmeshTotComplex));
        myfile.read((char*)&NmeshTotComplexSlice,   sizeof(NmeshTotComplexSlice));
        myfile.read((char*)&NmeshTotRealSlice,      sizeof(NmeshTotRealSlice));
        myfile.read((char*)&grid_is_in_real_space,  sizeof(grid_is_in_real_space));

        // Allocate and read main grid
        size_t bytes = sizeof(ComplexType) * NmeshTotComplexAlloc;
        fourier_grid_raw.resize(NmeshTotComplexAlloc);
        myfile.read((char*)fourier_grid_raw.data(), bytes);

        myfile.close();
      }


    //template class FFTWGrid<2>;
    //template class FFTWGrid<3>;
  }
}
#endif