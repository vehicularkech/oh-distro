/*
 * Segmentation.cpp
 *
 *  Created on: Jan 19, 2012
 *      Author: mfleder
 */

#include "Segmentation.h"
#include "PclSurrogateUtils.h"

#include <pcl/ModelCoefficients.h>
#include <pcl/point_types.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/passthrough.h>
#include <pcl/features/normal_3d.h>  //for computePointNormal
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>

#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/common/impl/centroid.hpp> //for computing centroid
#include <pcl/segmentation/extract_clusters.h>

using namespace std;
using namespace pcl;

namespace surrogate_gui
{


	vector<PointIndices::Ptr> Segmentation::segment(const PointCloud<PointXYZRGB>::ConstPtr cloud,
							boost::shared_ptr<set<int> >  subcloudIndices)
	{
	  //return getPlanarComponents(cloud, subcloudIndices);
	  
	  vector<PointIndices::Ptr> planeComponents = getRansacSegments(cloud,
									PclSurrogateUtils::toPclIndices(subcloudIndices),
									3, SACMODEL_PLANE);
	  
	  cout << endl << "***found " << planeComponents.size() << " plane components***" << endl;;

	  vector<PointIndices::Ptr> segments;
	  for (uint i = 0; i < planeComponents.size(); i++)
	    {
	      vector<PointIndices::Ptr> nextEuclidSegments = getEuclideanClusters(cloud, planeComponents[i]);

			//circle components
			/*for (uint n = 0; n < nextEuclidSegments.size(); n++)
			{
				vector<PointIndices::Ptr> nextCircleComponents = getRansacSegments(cloud, nextEuclidSegments[n],
																			   1, SACMODEL_CIRCLE2D);

				segments.insert(segments.end(), nextCircleComponents.begin(), nextCircleComponents.end());
			}*/

			//cout << endl << "plane component split into " << nextEuclidSegments.size() << " euclids" << endl;
			segments.insert(segments.end(), nextEuclidSegments.begin(), nextEuclidSegments.end());
		}


		cout << endl << "returning " << segments.size() << " segments" << endl;
		return segments;
	}

	/**@returns a vector of the largest segments having the given shape
	 * @param cloud we search a subset of this cloud
	 * @param subcloudIndices subset of cloud to search for shapeToFind shapes
	 * @param maxNumSegments maximum number of segments to return
	 * @param shapeToFind shape we are looking for.  eg pcl::SACMODEL_PLANE*/
	vector<PointIndices::Ptr> Segmentation::getRansacSegments(const PointCloud<PointXYZRGB>::ConstPtr cloud,
															  PointIndices::Ptr subCloudIndices,
															  uint maxNumSegments, pcl::SacModel shapeToFind)
	{
		//====copy data
		PointIndices::Ptr subcloudCopy = PclSurrogateUtils::copyIndices(subCloudIndices);

		//create the segmentation object
		SACSegmentation<PointXYZRGB> seg;
		seg.setOptimizeCoefficients(true); //optional
		seg.setModelType(shapeToFind);
		seg.setMethodType(SAC_RANSAC);
		seg.setDistanceThreshold(0.01);

		//filtering object
		vector<PointIndices::Ptr> segmentsFound;  //planar/circle components we will return

		//extract the top 5 planar segments, or stop if no good plane fits left
		uint min_plane_size = (int) (0.2 * subcloudCopy->indices.size());
		if (min_plane_size > 500 || min_plane_size < 300)
			min_plane_size = 500;

		for (uint i = 0;
				i < maxNumSegments
				&& subcloudCopy->indices.size() > min_plane_size;
			 i++)
		{

			//set input
			seg.setInputCloud(cloud);
			seg.setIndices(subcloudCopy);

			//segment
			ModelCoefficients::Ptr coefficients(new ModelCoefficients);
			PointIndices::Ptr nextSegmentIndices (new PointIndices);
			seg.segment(*nextSegmentIndices, *coefficients);

			//good shape-fitting component?  todo: check the fit
			if (nextSegmentIndices->indices.size() > min_plane_size)
			{
				/*cout << endl << "next model size: " << planeIndices->indices.size() << " points with "
						<< coefficients->values.size() << " coefficients" << endl
						<< "R^2 = " << getPlaneFitStatistics(coefficients, cloud, planeIndices) << endl;
				*/
				segmentsFound.push_back(nextSegmentIndices); //found a shape-fitting component
			}
			else
				break; //no big segments left

			//=====remove this shape's (plane's) indices from pclSubCloudIndices
			//remainingIndices = pclSubCloudIndices - planeIndices
			boost::shared_ptr<set<int> > remainingIndices (new set<int>(subcloudCopy->indices.begin(),
																		subcloudCopy->indices.end()));
			for(uint i = 0; i < nextSegmentIndices->indices.size(); i++)
			{
				remainingIndices->erase(nextSegmentIndices->indices[i]);
			}

			subcloudCopy = PclSurrogateUtils::toPclIndices(remainingIndices);
		}
		return segmentsFound;
	}


	/**@returns a clustering of indicesToCLuster.  indices are with respect to cloud*/
	vector<PointIndices::Ptr> Segmentation::getEuclideanClusters(const PointCloud<PointXYZRGB>::ConstPtr cloud,
																 PointIndices::Ptr indicesToCluster)
	{
		if (cloud == PointCloud<PointXYZRGB>::Ptr())
			throw SurrogateException("Null cloud passed to getEuclideanClusters");
		if (indicesToCluster->indices.size() == 0)
			throw SurrogateException("empty indices passed to getEuclideanClusters");

		//KdTree requires indices passed in as shared_ptr<vector<int>>
		//but PointIndices::Ptr is of type shared_pt<PointIndices>
		//so we need to copy the vector<int> indices and create an IndicesConstPtr
		boost::shared_ptr<vector<int> > constIndicesToCluster(new vector<int>(indicesToCluster->indices));
		if (constIndicesToCluster->size() == 0)
			throw SurrogateException("Empty constIndicesToCluster");

		//------
/*		cout << "\n PCL 1.5.1 throws seg fault when trying to euclidean cluster.  skipping euclidean clusterin"
				<< endl;
		vector<PointIndices::Ptr> unmodifiedReturn;
		unmodifiedReturn.push_back(indicesToCluster);
		return unmodifiedReturn;*/
		//----

		//KdTree object for the search method of extraction
		search::KdTree<PointXYZRGB>::Ptr tree (new search::KdTree<PointXYZRGB>);
		tree->setInputCloud(cloud, constIndicesToCluster);

		//euclidean cluster object
		EuclideanClusterExtraction<PointXYZRGB> euclid;
		euclid.setClusterTolerance(0.02); //2cm
		euclid.setMinClusterSize(min(100, (int) (indicesToCluster->indices.size()/2.0)));
		euclid.setMaxClusterSize(indicesToCluster->indices.size());

		/*
		cout << "\n minClusterSize = " << euclid.getMinClusterSize() << endl;
		cout << "\n maxClusterSize = " << euclid.getMaxClusterSize() << endl;
		cout << "\n Cluster Tolerance = " << euclid.getClusterTolerance() << endl;
		*/

		euclid.setSearchMethod(tree);

		//set input
		euclid.setInputCloud(cloud);
		euclid.setIndices(indicesToCluster);

		//extract
		vector<PointIndices> cluster_indices;
		//cout << "\n about to run exract" << endl;
		euclid.extract(cluster_indices);  //pcl 1.5.1 throwing an error here

		//cout << endl << "=======got " << cluster_indices.size() << " clusters" << endl;

		//=======convert to pointer
		vector<PointIndices::Ptr> clustersAsPtrs;
		clustersAsPtrs.reserve(cluster_indices.size());

		for (uint i = 0; i < cluster_indices.size(); i++)
		{
			PointIndices::Ptr nextCluster(new PointIndices); //space for copy
			nextCluster->indices = cluster_indices[i].indices; //copy
			clustersAsPtrs.push_back(nextCluster);
		}

		return clustersAsPtrs;
	}


	/**http://en.wikipedia.org/wiki/Coefficient_of_determination
	 * @return R^2 value
	 *
	 * http://docs.pointclouds.org/trunk/group__sample__consensus.html#gad3677a8e6b185643a2b9ae981e831c14
	 * */
	double Segmentation::getPlaneFitStatistics(ModelCoefficients::Ptr planeCoeffs,
											   const PointCloud<PointXYZRGB>::ConstPtr cloud,
											   PointIndices::Ptr planeIndices)
	{
		//===========================================
		//====================================defensive checks
		if (planeCoeffs->values.size() != 4)
			throw SurrogateException("Excpecting 4 plane coefficients");
		if (planeIndices->indices.size() < 3)
			throw SurrogateException("Can't have a plane w/ < 3 points");

		//===========confirm the plane fit
		Eigen::Vector4f planeParameters;
		float curvature;

		pcl::computePointNormal(*cloud, planeIndices->indices, planeParameters, curvature);

		//cout << "==============recomputed plane params" << endl;
		for (int i = 0; i < planeParameters.size(); i++)
		{
			if (abs(planeCoeffs->values[i] - planeParameters[i]) > 0.1)
				cout << "\n *****expected " << planeCoeffs->values[i]
				 << " got " << planeParameters[i]
				 << " | difference = " << (planeCoeffs->values[i] - planeParameters[i])
				 << endl;
		}

		//================================
		//===============now compute stats

		//determine centroid
		Eigen::Vector4f centroid4f;
		compute3DCentroid(*cloud, planeIndices->indices, centroid4f);
		PointXYZ centroid(centroid4f[0], centroid4f[1], centroid4f[2]);

		//---get sum of squared errors and total sum of squares
		double sumSquaredErrors = 0;
		double sumAbsErrors = 0;
		double ssTot = 0;
		double maxError = -1;
		double minError = 999999999;
		double maxDist = 0;
		double minDist = 1000;
		for(vector<int>::iterator indIter = planeIndices->indices.begin();
			indIter != planeIndices->indices.end();
			indIter++)
		{
			const PointXYZRGB &nextPoint = cloud->points[*indIter];
			double error = pcl::pointToPlaneDistance(nextPoint, //planeParameters);
													 planeCoeffs->values[0],
													 planeCoeffs->values[1],
													 planeCoeffs->values[2],
													 planeCoeffs->values[3]);
			sumSquaredErrors += error*error;
			sumAbsErrors += error;
			double distanceFromCentroid = euclideanDistance(centroid, nextPoint);
			ssTot += distanceFromCentroid*distanceFromCentroid;

			maxError = max(error, maxError);
			minError = min(error, minError);
			maxDist = max(distanceFromCentroid, maxDist);
			minDist = min(distanceFromCentroid, minDist);
		}


		//------compute R^2
		double meanError = sumAbsErrors/planeIndices->indices.size();
		cout << endl << " | sum^2 errors = " << sumSquaredErrors
			 << " | mean error = " << meanError
			 << " | max error = " << maxError << " | minError = " << minError << endl
			 << " | maxDistFromCentroid = " << maxDist << " | minDistFromCentroid = " << minDist << endl;

		return 1 - (sumSquaredErrors/ssTot);
	}


  //==============cylinder
  PointIndices::Ptr Segmentation::fitCylinder(const pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr cloud,
					      boost::shared_ptr<set<int> >  subcloudIndices,
					      double &x, double &y, double &z,
					      double &roll, double &pitch, double &yaw, 
					      double &radius,
					      double &height)
  {
    cout << "\n in fit cylinder.  num indices = " << subcloudIndices->size() << endl;
    cout << "\n cloud size = " << cloud->points.size() << endl;

    PointCloud<PointXYZRGB>::Ptr subcloud = PclSurrogateUtils::extractIndexedPoints(subcloudIndices, cloud);

    //---normals
    pcl::search::KdTree<PointXYZRGB>::Ptr tree (new pcl::search::KdTree<PointXYZRGB> ());
    NormalEstimation<PointXYZRGB, pcl::Normal> ne;
    ne.setSearchMethod (tree);
    ne.setInputCloud (subcloud);
    ne.setKSearch (50);
    PointCloud<pcl::Normal>::Ptr subcloud_normals (new PointCloud<pcl::Normal>);
    ne.compute (*subcloud_normals);

    //create the segmentation object
    SACSegmentationFromNormals<PointXYZRGB, pcl::Normal> seg;
    seg.setOptimizeCoefficients(true); //optional
    seg.setModelType(SACMODEL_CYLINDER); //pcl::SACMODEL_CYLINDER);
    seg.setMethodType(SAC_RANSAC);
    seg.setDistanceThreshold(0.05);
    seg.setRadiusLimits(0, 100);
       
    //set input
    seg.setInputCloud(subcloud);
    seg.setInputNormals(subcloud_normals);

    //segment
    ModelCoefficients::Ptr coefficients(new ModelCoefficients);
    PointIndices::Ptr cylinderIndices (new PointIndices);
    seg.segment(*cylinderIndices, *coefficients);
    
    //todo: xyz / rpyaw  / radius, height
    
    return cylinderIndices;
  }









	//==================
	//---------non-instantiable
	Segmentation::Segmentation()
	{
		throw exception();
	}

	Segmentation::~Segmentation()
	{
		throw exception();
	}


} //namespace surrogate_gui
