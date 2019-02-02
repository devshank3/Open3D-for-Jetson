//
// Created by wei on 1/11/19.
//

#pragma once

#include <Cuda/Common/LinearAlgebraCuda.h>
#include <Cuda/Container/ArrayCuda.h>
#include <Cuda/Geometry/PointCloudCuda.h>
#include <Cuda/Registration/CorrespondenceSetCuda.h>
#include <Core/Registration/TransformationEstimation.h>
#include <Core/Geometry/KDTreeFlann.h>

namespace open3d {
namespace cuda {

class RegistrationResultCuda {
public:
    RegistrationResultCuda(const Eigen::Matrix4d &transformation =
    Eigen::Matrix4d::Identity()) : transformation_(transformation),
                                   inlier_rmse_(0.0), fitness_(0.0) {}
    ~RegistrationResultCuda() {}

public:
    Eigen::Matrix4d transformation_;
    CorrespondenceSetCuda correspondences_;
    float inlier_rmse_;
    float fitness_;
};

class TransformEstimationCuda {
public:
    TransformEstimationCuda() {}
    virtual ~TransformEstimationCuda() {}

public:
    virtual TransformationEstimationType
    GetTransformationEstimationType() const = 0;

    virtual void Create() = 0;
    virtual void Release() = 0;
    virtual void UpdateDevice() = 0;
    virtual RegistrationResultCuda ComputeResultsAndTransformation() = 0;

    virtual void Initialize(PointCloud &source, PointCloud &target,
                            float max_correspondence_distance);
    virtual void GetCorrespondences();
    virtual void TransformSourcePointCloud(
        const Eigen::Matrix4d &source_to_target);

    virtual void ExtractResults(
        Eigen::Matrix6d &JtJ, Eigen::Vector6d &Jtr, float &rmse);

public:
    /** For GPU **/
    PointCloudCuda source_;
    PointCloudCuda target_;

    /** 1-nn, source x 1 (in target) **/
    CorrespondenceSetCuda correspondences_;
    float max_correspondence_distance_;

    /** For CPU NN search **/
    PointCloud source_cpu_;
    PointCloud target_cpu_;
    KDTreeFlann kdtree_;
    Eigen::MatrixXi corres_matrix_;

    /** Build linear system **/
    ArrayCuda<float> results_;
};

/* Only the CPU interface uses inheritance
 * vtable will face problems on the device side.
 * So the device classes looks a little bit redundant */
/***************************************/
/** Point to Point **/
class TransformEstimationPointToPointCudaDevice {
public:
    PointCloudCudaDevice source_;
    PointCloudCudaDevice target_;
    CorrespondenceSetCudaDevice correspondences_;

    ArrayCudaDevice<float> results_;
    TransformCuda transform_source_to_target_;

    Vector3f source_mean_;
    Vector3f target_mean_;

public:
    __DEVICE__ void ComputePointwiseStatistics(
        int source_idx, int target_idx,
        Matrix3f &Sigma, float &source_sigma2, float &residual);
};

class TransformEstimationPointToPointCuda : public TransformEstimationCuda {
public:
    std::shared_ptr<TransformEstimationPointToPointCudaDevice> device_
        = nullptr;

public:
    TransformEstimationPointToPointCuda(bool with_scaling = false) :
        with_scaling_(with_scaling) { Create(); }
    ~TransformEstimationPointToPointCuda() override { Release(); }

public:
    TransformationEstimationType GetTransformationEstimationType()
    const override { return type_; };

    void Create() override;
    void Release() override;
    void UpdateDevice() override;

    RegistrationResultCuda ComputeResultsAndTransformation() override;

    /* A Different linear system */
    void UnpackSums(Eigen::Vector3d &source_sum,
                    Eigen::Vector3d &target_sum);
    void UnpackSigmasAndRmse(Eigen::Matrix3d &Sigma,
                             float &source_sigma2, float &rmse);

public:
    bool with_scaling_ = false;

private:
    const TransformationEstimationType type_ =
        TransformationEstimationType::PointToPoint;
};

class TransformEstimationPointToPointCudaKernelCaller {
public:
    static void ComputeSums(
        TransformEstimationPointToPointCuda &estimation);
    static void ComputeResultsAndTransformation(
        TransformEstimationPointToPointCuda &estimation);
};

__GLOBAL__
void ComputeSumsKernel(
    TransformEstimationPointToPointCudaDevice estimation);
__GLOBAL__
void ComputeResultsAndTransformationKernel(
    TransformEstimationPointToPointCudaDevice estimation);

/***************************************/
/** PointToPlane **/
class TransformEstimationPointToPlaneCudaDevice {
public:
    PointCloudCudaDevice source_;
    PointCloudCudaDevice target_;
    CorrespondenceSetCudaDevice correspondences_;

    ArrayCudaDevice<float> results_;
    TransformCuda transform_source_to_target_;

public:
    __DEVICE__ void ComputePointwiseJacobianAndResidual(
        int source_idx, int target_idx,
        Vector6f& jacobian, float &residual);
};

class TransformEstimationPointToPlaneCuda : public TransformEstimationCuda {
public:
    std::shared_ptr<TransformEstimationPointToPlaneCudaDevice> device_
        = nullptr;
public:
    TransformEstimationPointToPlaneCuda() { Create(); };
    ~TransformEstimationPointToPlaneCuda() override { Release(); }

public:
    TransformationEstimationType GetTransformationEstimationType()
    const override { return type_; };

    void Create() override;
    void Release() override;
    void UpdateDevice() override;

    RegistrationResultCuda ComputeResultsAndTransformation() override;

private:
    const TransformationEstimationType type_ =
        TransformationEstimationType::PointToPlane;
};

class TransformEstimationPointToPlaneCudaKernelCaller {
public:
    static void ComputeResultsAndTransformation(
        TransformEstimationPointToPlaneCuda &estimation);
};

__GLOBAL__
void ComputeResultsAndTransformationKernel(
    TransformEstimationPointToPlaneCudaDevice estimation);
}
}