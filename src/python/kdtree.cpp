// SPDX-FileCopyrightText: Copyright 2024 Kenji Koide
// SPDX-License-Identifier: MIT
#include <iostream>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/eigen.h>
#include <pybind11/functional.h>

#include <small_gicp/points/point_cloud.hpp>
#include <small_gicp/ann/kdtree_omp.hpp>

namespace py = pybind11;
using namespace small_gicp;

void define_kdtree(py::module& m) {
  // KdTree
  py::class_<KdTree<PointCloud>, std::shared_ptr<KdTree<PointCloud>>>(m, "KdTree")  //
    .def(
      py::init([](const PointCloud::ConstPtr& points, int num_threads) { return std::make_shared<KdTree<PointCloud>>(points, KdTreeBuilderOMP(num_threads)); }),
      py::arg("points"),
      py::arg("num_threads") = 1,
      R"""(
         Construct a KdTree from a point cloud.

         Parameters
         ----------
         points : PointCloud
             The input point cloud.
         num_threads : int, optional
             The number of threads to use for KdTree construction. Default is 1.
       )""")

    .def(
      "nearest_neighbor_search",
      [](const KdTree<PointCloud>& kdtree, const Eigen::Vector3d& pt) {
        size_t k_index = -1;
        double k_sq_dist = std::numeric_limits<double>::max();
        const size_t found = traits::nearest_neighbor_search(kdtree, Eigen::Vector4d(pt.x(), pt.y(), pt.z(), 1.0), &k_index, &k_sq_dist);
        return std::make_tuple(found, k_index, k_sq_dist);
      },
      py::arg("pt"),
      R"""(
         Find the nearest neighbor to a given point.

         Parameters
         ----------
         pt : NDArray, shape (3,)
             The input point.

         Returns
         -------
         found : int
             Whether a neighbor was found (1 if found, 0 if not).
         k_index : int
             The index of the nearest neighbor in the point cloud.
         k_sq_dist : float
             The squared distance to the nearest neighbor.
       )""")

    .def(
      "knn_search",
      [](const KdTree<PointCloud>& kdtree, const Eigen::Vector3d& pt, int k) {
        std::vector<size_t> k_indices(k, -1);
        std::vector<double> k_sq_dists(k, std::numeric_limits<double>::max());
        const size_t found = traits::knn_search(kdtree, Eigen::Vector4d(pt.x(), pt.y(), pt.z(), 1.0), k, k_indices.data(), k_sq_dists.data());
        return std::make_pair(k_indices, k_sq_dists);
      },
      py::arg("pt"),
      py::arg("k"),
      R"""(
       Find the k nearest neighbors to a given point.

       Parameters
       ----------
       pt : NDArray, shape (3,)
           The input point.
       k : int
           The number of nearest neighbors to search for.

       Returns
       -------
       k_indices : NDArray, shape (k,)
           The indices of the k nearest neighbors in the point cloud.
       k_sq_dists : NDArray, shape (k,)
           The squared distances to the k nearest neighbors.
     )""")

    .def(
      "batch_nearest_neighbor_search",
      [](const KdTree<PointCloud>& kdtree, const Eigen::MatrixXd& pts, int num_threads) {
        if (pts.cols() != 3 && pts.cols() != 4) {
          throw std::invalid_argument("pts must have shape (n, 3) or (n, 4)");
        }

        std::vector<size_t> k_indices(pts.rows(), -1);
        std::vector<double> k_sq_dists(pts.rows(), std::numeric_limits<double>::max());

#pragma omp parallel for num_threads(num_threads)
        for (int i = 0; i < pts.rows(); ++i) {
          const size_t found = traits::nearest_neighbor_search(kdtree, Eigen::Vector4d(pts(i, 0), pts(i, 1), pts(i, 2), 1.0), &k_indices[i], &k_sq_dists[i]);
          if (!found) {
            k_indices[i] = -1;
            k_sq_dists[i] = std::numeric_limits<double>::max();
          }
        }

        return std::make_pair(k_indices, k_sq_dists);
      },
      py::arg("pts"),
      py::arg("num_threads") = 1,
      R"""(
       Find the nearest neighbors for a batch of points.

       Parameters
       ----------
       pts : NDArray, shape (n, 3) or (n, 4)
           The input points.
       num_threads : int, optional
           The number of threads to use for the search. Default is 1.

       Returns
       -------
       k_indices : NDArray, shape (n,)
           The indices of the nearest neighbors for each input point.
       k_sq_dists : NDArray, shape (n,)
           The squared distances to the nearest neighbors for each input point.
     )""")

    .def(
      "batch_knn_search",
      [](const KdTree<PointCloud>& kdtree, const Eigen::MatrixXd& pts, int k, int num_threads) {
        if (pts.cols() != 3 && pts.cols() != 4) {
          throw std::invalid_argument("pts must have shape (n, 3) or (n, 4)");
        }

        std::vector<std::vector<size_t>> k_indices(pts.rows(), std::vector<size_t>(k, -1));
        std::vector<std::vector<double>> k_sq_dists(pts.rows(), std::vector<double>(k, std::numeric_limits<double>::max()));

#pragma omp parallel for num_threads(num_threads)
        for (int i = 0; i < pts.rows(); ++i) {
          const size_t found = traits::knn_search(kdtree, Eigen::Vector4d(pts(i, 0), pts(i, 1), pts(i, 2), 1.0), k, k_indices[i].data(), k_sq_dists[i].data());
          if (found < k) {
            for (size_t j = found; j < k; ++j) {
              k_indices[i][j] = -1;
              k_sq_dists[i][j] = std::numeric_limits<double>::max();
            }
          }
        }

        return std::make_pair(k_indices, k_sq_dists);
      },
      py::arg("pts"),
      py::arg("k"),
      py::arg("num_threads") = 1,
      R"""(
       Find the k nearest neighbors for a batch of points.

       Parameters
       ----------
       pts : NDArray, shape (n, 3) or (n, 4)
           The input points.
       k : int
           The number of nearest neighbors to search for.
       num_threads : int, optional
           The number of threads to use for the search. Default is 1.

       Returns
       -------
       k_indices : list of NDArray, shape (n,)
           The list of indices of the k nearest neighbors for each input point.
       k_sq_dists : list of NDArray, shape (n,)
           The list of squared distances to the k nearest neighbors for each input point.
     )""");
}
