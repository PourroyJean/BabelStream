
// Copyright (c) 2015-16 Tom Deakin, Simon McIntosh-Smith,
// University of Bristol HPC
//
// For full license terms please see the LICENSE file distributed with this
// source code

#include "OMPStream.h"

#ifndef ALIGNMENT
#define ALIGNMENT (2*1024*1024) // 2MB
#endif

template <class T>
OMPStream<T>::OMPStream(const unsigned int ARRAY_SIZE, int device)
{

  array_size = ARRAY_SIZE;

//  int dev_id = omp_get_default_device();
//  this->a = (T*)omp_target_alloc(sizeof(T)*array_size, dev_id);
//  this->b = (T*)omp_target_alloc(sizeof(T)*array_size, dev_id);
//  this->c = (T*)omp_target_alloc(sizeof(T)*array_size, dev_id);
//  omp_set_default_device(dev_id);

  omp_set_default_device(device);

  // OpenMP has two memory spaces: host and device.
  // We allocate on the host and map to the device if required.

  // Allocate on the host
  a = (T*)aligned_alloc(ALIGNMENT, sizeof(T)*array_size);
  b = (T*)aligned_alloc(ALIGNMENT, sizeof(T)*array_size);
  c = (T*)aligned_alloc(ALIGNMENT, sizeof(T)*array_size);

  // Set up data region on device
  #pragma omp target enter data map(alloc: a[0:array_size], b[0:array_size], c[0:array_size])

}

template <class T>
OMPStream<T>::~OMPStream()
{
  // End data region on device
  unsigned int array_size = this->array_size;

  #pragma omp target exit data map(release: a[0:array_size], b[0:array_size], c[0:array_size])

  free(a);
  free(b);
  free(c);
}

template <class T>
void OMPStream<T>::init_arrays(T initA, T initB, T initC)
{
  #if defined(OMP_TARGET_GPU) && defined(__llvm__)
  // LLVM BUG: Need to pull out the pointers
  T *a = this->a;
  T *b = this->b;
  T *c = this->c;
  #endif

  #pragma omp target
  #pragma omp teams distribute parallel for simd
  for (int i = 0; i < array_size; i++)
  {
    a[i] = initA;
    b[i] = initB;
    c[i] = initC;
  }

  #if defined(OMP_TARGET_GPU) && defined(_CRAYC)
  // If using the Cray compiler, the kernels do not block, so this update forces
  // a small copy to ensure blocking so that timing is correct
  #pragma omp target update from(a[0:0])
  #endif
}

template <class T>
void OMPStream<T>::read_arrays(std::vector<T>& h_a, std::vector<T>& h_b, std::vector<T>& h_c)
{
  #if defined(OMP_TARGET_GPU) && defined(__llvm__)
  // LLVM BUG: Need to pull out the pointers
  T *a = this->a;
  T *b = this->b;
  T *c = this->c;
  #endif

  #pragma omp target update from(a[0:array_size], b[0:array_size], c[0:array_size])

  #pragma omp parallel for
  for (int i = 0; i < array_size; i++)
  {
    h_a[i] = a[i];
    h_b[i] = b[i];
    h_c[i] = c[i];
  }

}

template <class T>
void OMPStream<T>::copy()
{
  #if defined(OMP_TARGET_GPU) && defined(__llvm__)
  // LLVM BUG: Need to pull out the pointers
  T *a = this->a;
  T *c = this->c;
  #endif

  #pragma omp target
  #pragma omp teams distribute parallel for simd
  for (int i = 0; i < array_size; i++)
  {
    c[i] = a[i];
  }

  #if defined(OMP_TARGET_GPU) && defined(_CRAYC)
  // If using the Cray compiler, the kernels do not block, so this update forces
  // a small copy to ensure blocking so that timing is correct
  #pragma omp target update from(a[0:0])
  #endif
}

template <class T>
void OMPStream<T>::mul()
{
  const T scalar = startScalar;

  #if defined(OMP_TARGET_GPU) && defined(__llvm__)
  // LLVM BUG: Need to pull out the pointers
  T *b = this->b;
  T *c = this->c;
  #endif

  #pragma omp target
  #pragma omp teams distribute parallel for simd
  for (int i = 0; i < array_size; i++)
  {
    b[i] = scalar * c[i];
  }

  #if defined(OMP_TARGET_GPU) && defined(_CRAYC)
  // If using the Cray compiler, the kernels do not block, so this update forces
  // a small copy to ensure blocking so that timing is correct
  #pragma omp target update from(c[0:0])
  #endif
}

template <class T>
void OMPStream<T>::add()
{
  #if defined(OMP_TARGET_GPU) && defined(__llvm__)
  // LLVM BUG: Need to pull out the pointers
  T *a = this->a;
  T *b = this->b;
  T *c = this->c;
  #endif

  #pragma omp target
  #pragma omp teams distribute parallel for simd
  for (int i = 0; i < array_size; i++)
  {
    c[i] = a[i] + b[i];
  }

  #if defined(OMP_TARGET_GPU) && defined(_CRAYC)
  // If using the Cray compiler, the kernels do not block, so this update forces
  // a small copy to ensure blocking so that timing is correct
  #pragma omp target update from(a[0:0])
  #endif
}

template <class T>
void OMPStream<T>::triad()
{
  const T scalar = startScalar;

  #if defined(OMP_TARGET_GPU) && defined(__llvm__)
  // LLVM BUG: Need to pull out the pointers
  T *a = this->a;
  T *b = this->b;
  T *c = this->c;
  #endif

  #pragma omp target
  #pragma omp teams distribute parallel for simd
  for (int i = 0; i < array_size; i++)
  {
    a[i] = b[i] + scalar * c[i];
  }

  #if defined(OMP_TARGET_GPU) && defined(_CRAYC)
  // If using the Cray compiler, the kernels do not block, so this update forces
  // a small copy to ensure blocking so that timing is correct
  #pragma omp target update from(a[0:0])
  #endif
}

template <class T>
T OMPStream<T>::dot()
{

  T sum = 0.0;

  #if defined(OMP_TARGET_GPU) && defined(__llvm__)
  // LLVM BUG: Need to pull out the pointers
  T *a = this->a;
  T *b = this->b;
  #endif

  #pragma omp target map(tofrom: sum)
  #pragma teams distribute parallel for simd reduction(+:sum)
  for (int i = 0; i < array_size; i++)
  {
    sum += a[i] * b[i];
  }

  return sum;
}



void listDevices(void)
{
#ifdef OMP_TARGET_GPU
  // Get number of devices
  int count = omp_get_num_devices();

  // Print device list
  if (count == 0)
  {
    std::cerr << "No devices found." << std::endl;
  }
  else
  {
    std::cout << "There are " << count << " devices." << std::endl;
  }
#else
  std::cout << "0: CPU" << std::endl;
#endif
}

std::string getDeviceName(const int)
{
  return std::string("Device name unavailable");
}

std::string getDeviceDriver(const int)
{
  return std::string("Device driver unavailable");
}
template class OMPStream<float>;
template class OMPStream<double>;
