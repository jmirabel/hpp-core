/*
  Research carried out within the scope of the Associated International Laboratory: Joint Japanese-French Robotics Laboratory (JRL)

  Developed by Florent Lamiraux (LAAS-CNRS)
  and Mathieu Poirier (LAAS-CNRS)
*/

#define HPPPLANNER_DEFAULT_PENETRATION 0.05

// TODO : nettoyer les reste de la classe hppProblem


/*****************************************
 INCLUDES
*******************************************/

#include <iostream>
#include <fstream>

#include "KineoModel/kppDeviceComponent.h"

#include "hpp/core/planner.hh"
#include "hpp/core/problem.hh"
#include "hpp/core/parser.hh"
#include "humanoid-robot.hh"
#include "hppModel/hppBody.h"
#include <hppModel/hppHumanoidRobot.h>

#include "KineoUtility/kitNotificator.h"
#include "KineoWorks2/kwsDirectPath.h"
#include "KineoWorks2/kwsJoint.h"

#include "kprParserXML/kprParserManager.h"
#include "KineoModel/kppModelTree.h"
#include "KineoModel/kppDeviceNode.h"
#include "KineoModel/kppGeometryNode.h"
#include "KineoModel/kppPathNode.h"
#include "KineoModel/kppPathComponent.h"
#include "KineoModel/kppJointComponent.h"

#include "kwsPlus/roadmap/kwsPlusStopRdmBuilderDelegate.h"

KIT_PREDEF_CLASS(CkwsSteeringMethod);

const CkitNotification::TType
ChppPlanner::ID_HPP_ADD_ROBOT(CkitNotification::makeID());
const CkitNotification::TType
ChppPlanner::ID_HPP_SET_CURRENT_CONFIG(CkitNotification::makeID());
const CkitNotification::TType
ChppPlanner::ID_HPP_REMOVE_OBSTACLES(CkitNotification::makeID());
const CkitNotification::TType
ChppPlanner::ID_HPP_SET_OBSTACLE_LIST(CkitNotification::makeID());
const CkitNotification::TType
ChppPlanner::ID_HPP_ADD_OBSTACLE(CkitNotification::makeID());
const CkitNotification::TType
ChppPlanner::ID_HPP_REMOVE_ROADMAPBUILDER(CkitNotification::makeID());
const CkitNotification::TType
ChppPlanner::ID_HPP_ADD_ROADMAPBUILDER(CkitNotification::makeID());

const std::string ChppPlanner::ROBOT_KEY("robot");
const std::string ChppPlanner::OBSTACLE_KEY("obstacle");
const std::string ChppPlanner::CONFIG_KEY("config");
const std::string ChppPlanner::ROADMAP_KEY("roadmap");

#if DEBUG==3
#include "kwsioConfig.h"
#define ODEBUG3(x) std::cout << "ChppPlanner:" << x << std::endl
#define ODEBUG2(x) std::cout << "ChppPlanner:" << x << std::endl
#define ODEBUG1(x) std::cerr << "ChppPlanner:" << x << std::endl
#elif DEBUG==2
#include "kwsioConfig.h"
#define ODEBUG3(x)
#define ODEBUG2(x) std::cout << "ChppPlanner:" << x << std::endl
#define ODEBUG1(x) std::cerr << "ChppPlanner:" << x << std::endl
#elif DEBUG==1
#define ODEBUG3(x)
#define ODEBUG2(x)
#define ODEBUG1(x) std::cerr << "ChppPlanner:" << x << std::endl
#else
#define ODEBUG3(x)
#define ODEBUG2(x)
#define ODEBUG1(x)
#endif

/*****************************************
 PUBLIC METHODS
*******************************************/

// ==========================================================================

ChppPlanner::ChppPlanner()
{
  attNotificator = CkitNotificator::defaultNotificator();
  attObstacleList.clear();
  attStopRdmBuilderDelegate = new CkwsPlusStopRdmBuilderDelegate;
  parser_ = new hpp::core::Parser();
}

// ==========================================================================

ChppPlanner::ChppPlanner(const ChppPlanner& inPlanner) :
  attNotificator(inPlanner.attNotificator),
  hppProblemVector(inPlanner.hppProblemVector),
  attObstacleList(inPlanner.attObstacleList),
  attStopRdmBuilderDelegate
  (new CkwsPlusStopRdmBuilderDelegate(*attStopRdmBuilderDelegate)),
  parser_(NULL)
{
}

// ==========================================================================


ChppPlanner::~ChppPlanner()
{
  delete attStopRdmBuilderDelegate;
  if (parser_)
    delete parser_;
}

// ==========================================================================

ktStatus ChppPlanner::addHppProblem(CkppDeviceComponentShPtr inRobot,
				    double inPenetration)
{
  ChppProblem hppProblem(inRobot, attObstacleList, inPenetration);

  ODEBUG2(":addHppProblem: adding a problem in vector");
  // Add robot in vector .
  hppProblemVector.push_back(hppProblem);


  CkitNotificationShPtr notification
    = CkitNotification::createWithPtr<ChppPlanner>
    (ChppPlanner::ID_HPP_ADD_ROBOT, this);
  // set attribute if necessary
  notification->shPtrValue<CkppDeviceComponent>(ROBOT_KEY, inRobot);
  attNotificator->notify(notification);


  return KD_OK;
}

// ==========================================================================

ktStatus ChppPlanner::removeHppProblem()
{

  if(hppProblemVector.size()){
    hppProblemVector.pop_back();
    attObstacleList.clear();
    return KD_OK;
  }

  ODEBUG1(":removeHppProblem: no problem to remove.");
  return KD_ERROR;

}

// ==========================================================================

ktStatus
ChppPlanner::addHppProblemAtBeginning(CkppDeviceComponentShPtr inRobot,
				      double inPenetration)
{
  ChppProblem hppProblem(inRobot, attObstacleList, inPenetration);

  ODEBUG2(":addHppProblemAtBeginning: adding a problem");
  // Add robot in vector .
  hppProblemVector.push_front(hppProblem);

  CkitNotificationShPtr notification  =
    CkitNotification::createWithPtr<ChppPlanner>(ChppPlanner::ID_HPP_ADD_ROBOT,
						 this);
  // set attribute if necessary
  notification->shPtrValue<CkppDeviceComponent>(ROBOT_KEY, inRobot);
  attNotificator->notify(notification);


  return KD_OK;
}

// ==========================================================================

ktStatus ChppPlanner::removeHppProblemAtBeginning(){

  if(hppProblemVector.size()){
    hppProblemVector.pop_front();
    attObstacleList.clear();
    return KD_OK;
  }

  ODEBUG1(":removeHppProblem: no problem to remove.");
  return KD_ERROR;

}


// ==========================================================================

const CkppDeviceComponentShPtr
ChppPlanner::robotIthProblem(unsigned int rank) const
{
  CkppDeviceComponentShPtr nullShPtr;

  if (rank < getNbHppProblems()) {
    return hppProblemVector[rank].getRobot();
  }
  return nullShPtr;
}

// ==========================================================================

CkwsConfigShPtr ChppPlanner::robotCurrentConfIthProblem(unsigned int rank) const
{
  ktStatus status = KD_ERROR;
  CkwsConfigShPtr outConfig;

  if (rank < getNbHppProblems()) {
    const CkppDeviceComponentShPtr robot = robotIthProblem(rank);
    CkwsConfig config(robot);
    status = robot->getCurrentConfig(config);
    if (status == KD_OK) {
      outConfig = CkwsConfig::create(config);
    }
  }
  return outConfig;
}

// ==========================================================================

ktStatus ChppPlanner::robotCurrentConfIthProblem(unsigned int rank,
						 const CkwsConfigShPtr& config)
{
  ktStatus status = KD_ERROR;

  if (rank < getNbHppProblems()) {
    // status = hppProblemVector[rank].getRobot()->setCurrentConfig(config);
    // upadate it to use kppDeviceComponent
    status = hppProblemVector[rank].getRobot()->applyCurrentConfig(config);
  }

  /* send notification */
  if (status == KD_OK) {
    CkitNotificationShPtr notification
      = CkitNotification::createWithPtr<ChppPlanner>
      (ChppPlanner::ID_HPP_SET_CURRENT_CONFIG, this);
    notification->shPtrValue<CkwsConfig>(CONFIG_KEY, config);
    attNotificator->notify(notification);
  }

  return status;
}

// ==========================================================================

ktStatus ChppPlanner::robotCurrentConfIthProblem(unsigned int rank,
						 const CkwsConfig& config)
{
  ktStatus status = KD_ERROR;

  if (rank < getNbHppProblems()) {
    status = hppProblemVector[rank].getRobot()->setCurrentConfig(config);
  }


  // If success, send notification
  /* send notification */
  if (status == KD_OK) {
    // status = hppNotificator->sendNotification(ID_HPP_SET_CURRENT_CONFIG);
    CkitNotificationShPtr notification
      = CkitNotification::createWithPtr<ChppPlanner>
      (ChppPlanner::ID_HPP_SET_CURRENT_CONFIG, this);
    attNotificator->notify(notification);
    // set attribute if necessary

  }

  return status;
}

// ==========================================================================

CkwsConfigShPtr ChppPlanner::initConfIthProblem(unsigned int rank) const
{
  CkwsConfigShPtr config;

  if (rank < getNbHppProblems()) {
    config = hppProblemVector[rank].initConfig();
  }
  else {
    ODEBUG1(":initConfIthProblem: wrong problem id");
  }

  return config;
}

// ==========================================================================

ktStatus ChppPlanner::initConfIthProblem(unsigned int rank,
					 const CkwsConfigShPtr config)
{
  ktStatus status = KD_ERROR;

  if (rank < getNbHppProblems()) {
    status = hppProblemVector[rank].initConfig(config);
  }
  else {
    ODEBUG1(":initConfIthProblem: wrong problem id");
  }

  return status;
}

// ==========================================================================

CkwsConfigShPtr ChppPlanner::goalConfIthProblem(unsigned int rank) const
{
  CkwsConfigShPtr config;

  if (rank < getNbHppProblems()) {
    config = hppProblemVector[rank].goalConfig();
  }
  else {
    ODEBUG1(":goalConfIthProblem: wrong problem id");
  }

  return config;
}

// ==========================================================================

ktStatus ChppPlanner::goalConfIthProblem(unsigned int rank,
					 const CkwsConfigShPtr config)

{
  ktStatus status = KD_ERROR;

  if (rank < getNbHppProblems()) {
    status = hppProblemVector[rank].goalConfig(config);
  }
  else {
    ODEBUG1(":goalConfIthProblem: wrong problem id");
  }

  return status;
}

// ==========================================================================


ktStatus
ChppPlanner::roadmapBuilderIthProblem(unsigned int rank,
				      CkwsRoadmapBuilderShPtr inRoadmapBuilder,
				      bool inDisplay)
{
  if (rank >= getNbHppProblems()) {
    ODEBUG1
      (":roadmapBuilderIthProblem: "
       << "rank should be less than number of problems.");
    return KD_ERROR;
  }

  /*
    If a roadmap was already stored, it will be removed. If this roadmap is
    displayed in the interface, we need first to remove the corresponding
    data-structure in the interface. This is done by sending a notification.
  */
  CkitNotificationShPtr notification  =
    CkitNotification::createWithPtr<ChppPlanner>
    (ChppPlanner::ID_HPP_REMOVE_ROADMAPBUILDER, this);
  notification->unsignedIntValue(ChppPlanner::ROADMAP_KEY, rank);
  attNotificator->notify(notification);

  /*
    Add an interruption delegate to the roadmap builder
  */
#if 0
  inRoadmapBuilder->addDelegate(attStopRdmBuilderDelegate);
#endif
  ChppProblem& hppProblem =  hppProblemVector[rank];
  hppProblem.roadmapBuilder(inRoadmapBuilder);

  /*
     If the new roadmap is displayed in the interface, send a notification to
     trigger appropriate action.
  */
  if (inDisplay) {
    notification =
      CkitNotification::createWithPtr<ChppPlanner>
      (ChppPlanner::ID_HPP_ADD_ROADMAPBUILDER, this);
    notification->unsignedIntValue(ChppPlanner::ROADMAP_KEY, rank);
    attNotificator->notify(notification);
  }

  return KD_OK;
}

// ==========================================================================

CkwsRoadmapBuilderShPtr ChppPlanner::roadmapBuilderIthProblem(unsigned int rank)
{
  CkwsRoadmapBuilderShPtr roadmapBuilder;
  if (rank < getNbHppProblems()) {
    ChppProblem& hppProblem =  hppProblemVector[rank];
    roadmapBuilder = hppProblem.roadmapBuilder();
  }
  return roadmapBuilder;
}

// ==========================================================================

ktStatus
ChppPlanner::pathOptimizerIthProblem(unsigned int rank,
				     CkwsPathOptimizerShPtr inPathOptimizer)
{
  if (rank >= getNbHppProblems()) {
    ODEBUG1
      (":pathOptimizerIthProblem: "
       << "rank should be less than number of problems.");
    return KD_ERROR;
  }

  ChppProblem& hppProblem =  hppProblemVector[rank];
  hppProblem.pathOptimizer(inPathOptimizer);

  return KD_OK;
}


// ==========================================================================

CkwsPathOptimizerShPtr ChppPlanner::pathOptimizerIthProblem(unsigned int rank)
{
  CkwsPathOptimizerShPtr pathOptimizer;
  if (rank < getNbHppProblems()) {
    ChppProblem& hppProblem =  hppProblemVector[rank];
    pathOptimizer = hppProblem.pathOptimizer();
  }
  return pathOptimizer;
}

// ==========================================================================


ktStatus
ChppPlanner::steeringMethodIthProblem(unsigned int rank,
				      CkwsSteeringMethodShPtr inSM)
{

  if (rank >= getNbHppProblems()) {
    ODEBUG1
      (":steeringMethodIthProblem: "
       << "rank should be less than number of problems.");
    return KD_ERROR;
  }

  hppProblemVector[rank].getRobot()->steeringMethod(inSM) ;

  return KD_OK ;
}

// ==========================================================================

CkwsSteeringMethodShPtr ChppPlanner::steeringMethodIthProblem(unsigned int rank)
{

  CkwsSteeringMethodShPtr inSM ;
  if (rank < getNbHppProblems()) {
    inSM = hppProblemVector[rank].getRobot()->steeringMethod() ;
  }
  return inSM ;

}

// ==========================================================================

ktStatus ChppPlanner::configExtractorIthProblem(unsigned int inRank,
						const CkwsConfigExtractorShPtr&
						inConfigExtractor)
{
  if (inRank >= getNbHppProblems()) {
    ODEBUG1
      (":configExtractorIthProblem: "
       << "rank should be less than number of problems.");
    return KD_ERROR;
  }

  hppProblemVector[inRank].configExtractor(inConfigExtractor);
  return KD_OK ;
}

// ==========================================================================

ktStatus ChppPlanner::penetration(unsigned int inRank, double inPenetration)
{
  if (inRank >= getNbHppProblems()) {
    ODEBUG1(":penetration: rank should be less than number of problems.");
    return KD_ERROR;
  }
  hppProblemVector[inRank].penetration(inPenetration);
  return KD_OK;
}

// ==========================================================================

double ChppPlanner::penetration(unsigned int inRank) const
{
  if (inRank >= getNbHppProblems()) {
    ODEBUG1(":penetration: rank should be less than number of problems.");
    return KD_ERROR;
  }
  return   hppProblemVector[inRank].penetration();
}

// ==========================================================================

ktStatus ChppPlanner::obstacleList(std::vector<CkcdObjectShPtr> collisionList)
{
  // Send notification to destroy current obstacles in Kpp.

  CkitNotificationShPtr notification  =
    CkitNotification::createWithPtr<ChppPlanner>
    (ChppPlanner::ID_HPP_REMOVE_OBSTACLES, this);
  attNotificator->notify(notification);

  // Set list of obstacles.
  attObstacleList = collisionList;

  // Set the list of obstacles for each robot.
  unsigned int nProblem = getNbHppProblems();
  for (unsigned int iProblem=0; iProblem<nProblem; iProblem++) {
    ChppProblem& problem = hppProblemVector[iProblem];
    problem.obstacleList(collisionList);
  }

  // Send notification for new list of obstacles.
  notification = CkitNotification::createWithPtr<ChppPlanner>
    (ChppPlanner::ID_HPP_SET_OBSTACLE_LIST, this);
  // set attributes if necessary
  notification->ptrValue< std::vector<CkcdObjectShPtr> >
    (OBSTACLE_KEY, &attObstacleList);
  attNotificator->notify(notification);

  return KD_OK;
}

// ==========================================================================

const std::vector< CkcdObjectShPtr > ChppPlanner::obstacleList()
{
  return attObstacleList;
}

// ==========================================================================

ktStatus ChppPlanner::addObstacle(CkcdObjectShPtr object,
				  bool inDistanceComputation)
{
  attObstacleList.push_back(object);

  // Set the list of obstacles for each robot.
  unsigned int nProblem = getNbHppProblems();
  for (unsigned int iProblem=0; iProblem<nProblem; iProblem++) {
    ChppProblem& problem = hppProblemVector[iProblem];
    problem.addObstacle(object, inDistanceComputation);
  }

  // Send notification
  CkitNotificationShPtr notification =
    CkitNotification::createWithPtr<ChppPlanner>
    (ChppPlanner::ID_HPP_ADD_OBSTACLE, this);
  // set attributes if necessary
  notification->ptrValue< std::vector<CkcdObjectShPtr> >
    (OBSTACLE_KEY, &attObstacleList);
  attNotificator->notify(notification);

  return KD_OK;
}

// ==========================================================================

ktStatus ChppPlanner::solveOneProblem(unsigned int inRank)
{

  if (inRank >= getNbHppProblems()) {
    ODEBUG1(":solveOneProblem: problem Id=" << inRank
	    << "is bigger than vector size=" << getNbHppProblems());

    return KD_ERROR;
  }

  ChppProblem& hppProblem = hppProblemVector[inRank];

  return hppProblem.solve();
}

ktStatus
ChppPlanner::optimizePath(unsigned int inProblemId, unsigned int inPathId)
{
  if (inProblemId >= getNbHppProblems()) {
    ODEBUG1(":optimizePath: problem Id=" << inProblemId
	    << " is bigger than vector size=" << getNbHppProblems());

    return KD_ERROR;
  }

  ChppProblem& hppProblem = hppProblemVector[inProblemId];
  CkwsRoadmapBuilderShPtr roadmapBuilder = hppProblem.roadmapBuilder();
  if (!roadmapBuilder) {
    ODEBUG1
      (":optimizePath: roadmap builder should be set to define penetration.");
    return KD_ERROR;
  }
  double penetration = hppProblem.penetration();

  if (inPathId >= hppProblem.getNbPaths()) {
    ODEBUG1(":optimizePath: problem Id="
	    << inPathId << " is bigger than number of paths="
	    << hppProblem.getNbPaths());
    return KD_ERROR;
  }
  CkwsPathShPtr kwsPath = hppProblem.getIthPath(inPathId);

  // optimizer for the path
  if (hppProblem.pathOptimizer()) {
    hppProblem.pathOptimizer()->optimizePath(kwsPath, penetration);

    ODEBUG2(":optimizePath: path optimized with penetration "
	    << penetration);

  } else {
    ODEBUG1(":optimizePath: no optimizer defined");
  }
  return KD_OK;
}

// ==========================================================================

unsigned int ChppPlanner::getNbPaths(unsigned int inProblemId) const
{
  unsigned int nbPaths = 0;
  if (inProblemId < getNbHppProblems()) {
    nbPaths =  hppProblemVector[inProblemId].getNbPaths();
  } else {
    ODEBUG1(":getNbPaths : inProblemId = "<< inProblemId
	    << " should be smaller than nb of problems: "
	    << getNbHppProblems());
  }
  return nbPaths;
}

// ==========================================================================

CkwsPathShPtr ChppPlanner::getPath(unsigned int inProblemId,
                                   unsigned int inPathId) const
{
  CkwsPathShPtr resultPath;

  if (inProblemId < getNbHppProblems()) {
    if (inPathId < hppProblemVector[inProblemId].getNbPaths()) {
      resultPath = hppProblemVector[inProblemId].getIthPath(inPathId);
    }
  }
  return resultPath;
}

// ==========================================================================

ktStatus ChppPlanner::addPath(unsigned int inProblemId, CkwsPathShPtr kwsPath)
{
  if (inProblemId >= hppProblemVector.size()) {
    ODEBUG1(":addPath : inProblemId bigger than vector size.");
    return KD_ERROR;
  }
  hppProblemVector[inProblemId].addPath(kwsPath);
  return KD_OK;
}

// ==========================================================================

CkwsKCDBodyConstShPtr
ChppPlanner::findBodyByJointName(const std::string& inJointName) const
{
  CkwsKCDBodyConstShPtr kcdBody;
  unsigned int nbProblems = getNbHppProblems();

  // Loop over hppProblem.
  for (unsigned int iProblem=0; iProblem < nbProblems; iProblem++) {
    CkwsDevice::TJointVector jointVector;
    const CkppDeviceComponentShPtr hppRobot = robotIthProblem(iProblem);
    hppRobot->getJointVector(jointVector);

    // Loop over bodies of the robot.
    for (CkwsDevice::TJointIterator jointIter=jointVector.begin();
	 jointIter < jointVector.end();
	 jointIter++) {
      // Cast joint into CkppJointComponent
      if (CkppJointComponentShPtr kppJointInRobot =
	  KIT_DYNAMIC_PTR_CAST(CkppJointComponent, *jointIter)) {
	if (kppJointInRobot->name() == inJointName) {
	  kcdBody =
	    KIT_DYNAMIC_PTR_CAST(const CkwsKCDBody,
				 kppJointInRobot->kwsJoint()->attachedBody());
	  return kcdBody;
	}
      } else {
	ODEBUG1(":findBodyByJointName : one body of robot in hppProblem "
		<< iProblem << " is not a CkppJointComponent.");
      }
    }
  }
  return kcdBody;
}


ktStatus ChppPlanner::solve()
{
  ktStatus success=KD_OK;
  for (unsigned int iProblem=0; iProblem<getNbHppProblems(); iProblem++) {
    if (solveOneProblem(iProblem) != KD_OK) {
      success = KD_ERROR;
    }
  }
  return success;
}

void ChppPlanner::interruptPathPlanning()
{
  if (attStopRdmBuilderDelegate == NULL) {
    ODEBUG1(":interruptPathPlanning: no stop delegate.");
    return;
  }
  attStopRdmBuilderDelegate->shouldStop(true);
}

ktStatus ChppPlanner::parseFile(const std::string& inFileName)
{

  CkppComponentShPtr modelTreeComponent;
  CkprParserManagerShPtr parser = CkprParserManager::defaultManager();

  if(inFileName.length() == 0) {
    ODEBUG1(":parseFile: no file name");
    return KD_ERROR;
  }

  if(KD_ERROR == parser->loadComponentFromFile(inFileName,
					       modelTreeComponent)) {
    ODEBUG1(":parseFile: unable to load file " << inFileName);
    ODEBUG2(":parseFile: " << parser->lastError().errorMessage());
    return KD_ERROR;
  }

  CkppModelTreeShPtr modelTree =
    KIT_DYNAMIC_PTR_CAST(CkppModelTree,modelTreeComponent);

  CkppSolidComponentShPtr solidComponent =
    KIT_DYNAMIC_PTR_CAST(CkppSolidComponent,modelTreeComponent);

  if(modelTree) {
    ODEBUG1(":parseFile: found modelTree");


    std::map<CkppDeviceComponentShPtr,unsigned int> devicesIndex;

    std::set<CkcdObjectShPtr> devicesBodies;

    unsigned int currentRank=0;

    if(!modelTree->deviceNode()) std::cout << "No devices" << std::endl;
    else {
      for(unsigned int i = 0;
	  i< modelTree->deviceNode()->countChildComponents(); i++) {

	CkppDeviceComponentShPtr deviceComponent =
	  KIT_DYNAMIC_PTR_CAST(CkppDeviceComponent,
			       modelTree->deviceNode()->childComponent(i));

	if(deviceComponent) {
	  // If device is of type io::hpp::core::HumanoidRobot, transform it
	  // into a ChppHumanoidRobot
	  hpp::core::io::HumanoidRobotShPtr humanoidRobot =
	    KIT_DYNAMIC_PTR_CAST(hpp::core::io::HumanoidRobot, deviceComponent);
	  if (humanoidRobot) {
	    // Set config to 0
	    CkwsConfig config(deviceComponent);
	    deviceComponent->setCurrentConfig(config);
	    deviceComponent = humanoidRobot->createHppHumanoidRobot();
	    if (!deviceComponent) return KD_ERROR;
	  }
	  addHppProblem(deviceComponent, HPPPLANNER_DEFAULT_PENETRATION);
	  devicesIndex.insert
	    (std::pair<CkppDeviceComponentShPtr,unsigned int>(deviceComponent,
							      currentRank));

	  std::vector< CkppSolidComponentRefShPtr > solidComponentRefVector;

	  deviceComponent->getSolidComponentRefVector (solidComponentRefVector);

	  for(std::vector< CkppSolidComponentRefShPtr >::iterator
		it=solidComponentRefVector.begin();
	      it!=solidComponentRefVector.end();it++) {
	    CkcdObjectShPtr kcdObject =
	      KIT_DYNAMIC_PTR_CAST(CkcdObject,
				   (*it)->referencedSolidComponent());
	    if(kcdObject) devicesBodies.insert(kcdObject);
	    else std::cout << "Cannot cast component to kcdObject"
			   << std::endl;
	  }
	  currentRank++;
	}
      }
    }
    if(!modelTree->geometryNode()) std::cout<<"No geometries"<<std::endl;
    else{
      std::cout<<"geometries"<<std::endl;
      for(unsigned int i = 0;
	  i< modelTree->geometryNode()->countChildComponents(); i++){
	std::cout<<i<<" "<<std::endl;
	CkcdObjectShPtr kcdObject =
	  KIT_DYNAMIC_PTR_CAST(CkcdObject,
			       modelTree->geometryNode()->childComponent(i));
	if(kcdObject) addObstacle(kcdObject);
	else std::cout<<"Cannot cast component to kcdObject"<<std::endl;
      }
    }

    if(!modelTree->pathNode()) std::cout<<"No paths"<<std::endl;
    else {
      for(unsigned int i = 0; i< modelTree->pathNode()->countChildComponents();
	  i++){
	CkppPathComponentShPtr pathComponent =
	  KIT_DYNAMIC_PTR_CAST(CkppPathComponent,
			       modelTree->pathNode()->childComponent(i));

	std::map<CkppDeviceComponentShPtr,unsigned int>::iterator it =
	  devicesIndex.find(pathComponent->deviceComponent());

	if(it==devicesIndex.end()) {
	  ODEBUG1(":parseFile: no device matching path");
	  return KD_ERROR;
	}

	unsigned int rank=it->second;

	addPath(rank,pathComponent->kwsPath());

      }

    }
  }


  else if(solidComponent)
    {
      ODEBUG1(":parseFile: found solid component");
      CkcdObjectShPtr kcdObject =
	KIT_DYNAMIC_PTR_CAST(CkcdObject,solidComponent);
      if(!kcdObject) {
	  ODEBUG1(":parseFile: cannot cast solidComponent to kcdObject");
	  return KD_ERROR;
	}
      addObstacle(kcdObject);
    }

  else {
      ODEBUG1(":parseFile: cannot cast file neither to CkppSolidComponent "
	      << "nor to CkppModelTree ");
      return KD_ERROR;
    }

  ODEBUG1(":parseFile: everything is fine ");
  return KD_OK ;
}
