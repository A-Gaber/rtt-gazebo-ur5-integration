#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>

#include <rtt/Component.hpp>
#include <rtt/Port.hpp>
#include <rtt/TaskContext.hpp>
#include <rtt/Logger.hpp>
#include <rtt/Property.hpp>
#include <rtt/Attribute.hpp>

//#include <Eigen/Dense>
#include <boost/graph/graph_concepts.hpp>
#include <rtt/os/Timer.hpp>
#include <rtt/os/TimeService.hpp>
#include <boost/thread/mutex.hpp>

#include <rci/dto/JointAngles.h>
#include <rci/dto/JointTorques.h>
#include <rci/dto/JointVelocities.h>

#include <nemo/Vector.h>
#include <nemo/Mapping.h>

#include <fstream>
#include <iostream>
#include <math.h>

using namespace std;

#define l(lvl) RTT::log(lvl) << "[" << this->getName() << "] "

class UR5RttGazeboComponent: public RTT::TaskContext {
public:

	UR5RttGazeboComponent(std::string const& name) :
			RTT::TaskContext(name), nb_static_joints(
					0) , last_update_time_(0) // HACK: The urdf has static tag for base_link, which makes it appear in gazebo as a joint.
			 {
		// Add required gazebo interfaces.
		this->provides("gazebo")->addOperation("configure",
				&UR5RttGazeboComponent::gazeboConfigureHook, this,
				RTT::ClientThread);
		this->provides("gazebo")->addOperation("update",
				&UR5RttGazeboComponent::gazeboUpdateHook, this, RTT::ClientThread);

//		this->addOperation("setLinkGravityMode",&UR5RttGazeboComponent::setLinkGravityMode,this,RTT::ClientThread); // TODO

		nb_iteration = 0;
		sim_id = 1;

		//Test angle variation
		angle0 = 0;
		angle1 = -0.1;
		angle2 = 3.14 - (+ angle1 + acos(sin(-angle1)*l1/l2) + 1.57) - 0.3 -0.4;
		angle3 = -3.14;
		angle4 = -1.4;
		angle5 = -1.57;

		l1 = 0.7; // find real values later !!
		l2 = 0.9;// find real values later !!

		this->provides("debug")->addAttribute("jnt_pos", jnt_pos_);
		this->provides("debug")->addAttribute("jnt_vel", jnt_vel_);
		this->provides("debug")->addAttribute("jnt_trq", jnt_trq_);
		this->provides("misc")->addAttribute("urdf_string", urdf_string);

	}

	//! Called from gazebo
	virtual bool gazeboConfigureHook(gazebo::physics::ModelPtr model) {
		if (model.get() == NULL) {
			RTT::log(RTT::Error) << "No model could be loaded" << RTT::endlog();
			std::cout << "No model could be loaded" << RTT::endlog();
			return false;
		}



		// Get the joints
		gazebo_joints_ = model->GetJoints();
		model_links_ = model->GetLinks(); // Only working when starting gzserver and gzclient separately!

		RTT::log(RTT::Warning) << "Model has " << gazebo_joints_.size()
				<< " joints" << RTT::endlog();

		//NOTE: Get the joint names and store their indices
		// Because we have base_joint (fixed), j0...j6, ati_joint (fixed)
		int idx = 0;
		for (gazebo::physics::Joint_V::iterator jit = gazebo_joints_.begin();
				jit != gazebo_joints_.end(); ++jit, ++idx) {

			const std::string name = (*jit)->GetName();
			// NOTE: Remove fake fixed joints (revolute with upper==lower==0
			// NOTE: This is not used anymore thanks to <disableFixedJointLumping>
			// Gazebo option (ati_joint is fixed but gazebo can use it )

//			RTT::log(RTT::Warning) << "Found joint [" << name << "] idx:"
//								<< idx << RTT::endlog();

			if ((*jit)->GetLowerLimit(0u) == (*jit)->GetUpperLimit(0u)) {
				RTT::log(RTT::Warning) << "Not adding (fake) fixed joint ["
						<< name << "] idx:" << idx << RTT::endlog();
				continue;
			}
			joints_idx.push_back(idx);
			joint_names_.push_back(name);
			RTT::log(RTT::Warning) << "Adding joint [" << name << "] idx:"
					<< idx << RTT::endlog();
			std::cout << "Adding joint [" << name << "] idx:" << idx
					<< RTT::endlog();
		}

		if (joints_idx.size() == 0) {
			RTT::log(RTT::Error) << "No Joints could be added, exiting"
					<< RTT::endlog();
			return false;
		}

		RTT::log(RTT::Warning) << "Gazebo model found " << joints_idx.size()
				<< " joints " << RTT::endlog();


		jnt_pos_ = rci::JointAngles::create(7, 0.0);

		jnt_trq_ = rci::JointTorques::create(7, 0.0);

		jnt_vel_ = rci::JointVelocities::create(7, 0.0);


		last_update_time_ = RTT::os::TimeService::Instance()->getNSecs(); //rtt_rosclock::rtt_now(); // still needed??
		RTT::log(RTT::Warning) << "Done configuring gazebo" << RTT::endlog();

		for (unsigned j = 0; j < joints_idx.size(); j++)
		{
			gazebo_joints_[joints_idx[j]]->SetProvideFeedback(true);
		}

		data_file.open("/homes/abalayn/workspace/test_data.txt", std::ios::out);

		return true;
	}

	//! Called from Gazebo
	virtual void gazeboUpdateHook(gazebo::physics::ModelPtr model) {
		if (model.get() == NULL) {
			return;
		}




		// Position of each joint (in radian)
			//gazebo_joints_[joints_idx[0]]->SetPosition(0 , -0);
			//angle1 = -0.5;
			//gazebo_joints_[joints_idx[1]]->SetPosition(0 , angle1);
			//angle2 = 3.14 - (+ angle1 + acos(sin(-angle1)*l1/l2) + 1.57); // OK (just subb pi)! for angle1 small
			//angle2 = -3.14 + (- angle1 + 1.7 - acos(cos(-angle1+1.7)*l1/l2)); // Ok for big angle1 (add pi)
			//angle2= -0.7;
			//gazebo_joints_[joints_idx[2]]->SetPosition(0 , angle2);
			//gazebo_joints_[joints_idx[3]]->SetPosition(0 , -1.55);
			//gazebo_joints_[joints_idx[4]]->SetPosition(0 , -0);
			//gazebo_joints_[joints_idx[5]]->SetPosition(0 , -0);




		//angle = fmod((angle + 0.005),3);
			nb_iteration ++;
			if (nb_iteration >= 100) // For stabilisation of the torque
			{

				data_file << "{ sim_id = " << sim_id << " ; ";

				for (unsigned j = 0; j < joints_idx.size(); j++)
				{
					data_file << "joint = " << j << " ; ";
					gazebo::physics::JointWrench w1 = gazebo_joints_[joints_idx[j]]->GetForceTorque(0u);
					gazebo::math::Vector3 a1 = gazebo_joints_[joints_idx[j]]->GetLocalAxis(0u);
					//data_file << "torque parent 1 = "<< w1.body1Torque << std::endl;
					//data_file << "torque child 1 = "<< w1.body2Torque << std::endl;
					jnt_trq_->setFromNm(1, a1.Dot(w1.body1Torque));
					data_file << "trq = "<< a1.Dot(w1.body1Torque) << " ; "; // See torque computation !!
					data_file << "trq2 = "<< a1.Dot(w1.body2Torque) << " ; "; // See torque computation !!
					data_file << "angle = "	<< model->GetJoints()[joints_idx[j]]->GetAngle(0).Radian() << " ; ";
				}
				data_file << " }" << std::endl;
				//angle = fmod((angle - 0.005),2.5);
				nb_iteration = 0;
				sim_id ++;



				// Change values of angles to collect data for the simulation.

				//gazebo_joints_[joints_idx[1]]->SetPosition(0 , angle2);
				//RTT::log(RTT::Warning) << angle2 << RTT::endlog();

	if (angle5 < 3.14)
	{
		angle5 = angle5 + 1.17;
	}
	else
	{
		  if (angle4 < 1.57)
		  {
			angle4 = angle4 + 0.7;
		  }
		  else
		  {
			if ( (angle3 <(0.7)))
			{
				angle3 = angle3 + 0.9;
			}
			else
			{

				if (angle1 < 1.57 && angle2 > (3.14 - (+ angle1 + acos(sin(-angle1)*l1/l2) + 1.57) - 3.14 + 0.8))
				{
					angle2 = angle2 - 0.3;
				}
				else if (angle1 > 1.57 && angle2 > (-3.14 + (- angle1 + 1.7 - acos(cos(-angle1+1.7)*l1/l2))+3.14 - 0.8))
				{
					angle2 = angle2 + 0.3;
				}
				else
				{
					if (angle1 > -2.3)
					{
						angle1 = angle1 - 0.4;
					}
					else
					{
						angle1 = -0.1;
						if (angle0 >= 6.28)
						{
							angle0 = 0;
						}
						else
						{
							angle0 = angle0 + 1.5;
						}
					}
					if (angle1 < 1.57)
					{
						angle2 = 3.14 - (+ angle1 + acos(sin(-angle1)*l1/l2) + 1.57) - 0.3 -0.4;
					}
					else
					{
						angle2 = -3.14 + (- angle1 + 1.7 - acos(cos(-angle1+1.7)*l1/l2)) + 0.3 +0.4;
					}
				}
				angle3 = -3.14;
			}
			angle4 = -1.4;
				  }
		  angle5 = -1.57;
	}
			gazebo_joints_[joints_idx[0]]->SetPosition(0 , angle0);
			//gazebo_joints_[joints_idx[1]]->SetPosition(0 , angle1);
			gazebo_joints_[joints_idx[2]]->SetPosition(0 , angle2);
			gazebo_joints_[joints_idx[3]]->SetPosition(0 , angle3);
			gazebo_joints_[joints_idx[4]]->SetPosition(0 , angle4);
			gazebo_joints_[joints_idx[5]]->SetPosition(0 , angle5);

			gazebo_joints_[joints_idx[1]]->SetAngle(0 , angle1);
			}
			// gazebo_joints_[joints_idx[4]]->SetForce(0 , 1000); test -> Works ! position should be set with torques !
	}

	virtual bool configureHook() {
		return true;
	}


	virtual void updateHook() {
		return;
	}
protected:

	//! Synchronization ??

	// File where data are written.
	std::ofstream data_file;

	int nb_iteration; // number of hook iterations for one tested position.
	int sim_id; // number of angle positions tested.

	std::vector<int> joints_idx;

	std::map<gazebo::physics::LinkPtr, bool> gravity_mode_;

	std::vector<gazebo::physics::JointPtr> gazebo_joints_;
	gazebo::physics::Link_V model_links_;
	std::vector<std::string> joint_names_;


	rci::JointAnglesPtr  jnt_pos_;
	rci::JointTorquesPtr jnt_trq_;
	rci::JointVelocitiesPtr jnt_vel_;


	RTT::nsecs last_update_time_;

	int nb_static_joints;

	// contains the urdf string for the associated model.
	std::string urdf_string;


	// Test : variation of angle for each joint
	double angle0;
	double angle1;
	double angle2;
	double angle3;
	double angle4;
	double angle5;

	double l1; // length of link1
	double l2;  // length of link2
};

ORO_LIST_COMPONENT_TYPE(UR5RttGazeboComponent)
ORO_CREATE_COMPONENT_LIBRARY();
