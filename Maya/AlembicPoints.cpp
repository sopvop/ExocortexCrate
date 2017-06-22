#include "stdafx.h"

#include "AlembicPoints.h"
#include "AttributesReading.h"
#include "MetaData.h"

#include <maya/MArrayDataBuilder.h>

bool AlembicPoints::listIntanceNames(std::vector<std::string> &names)
{
  MStatus status;
  MDagPathArray allPaths;
  {
    MMatrixArray allMatrices;
    MIntArray pathIndices;
    MIntArray pathStartIndices;

    MSelectionList sl;
    sl.add(instName);

    MDagPath dagp;
    sl.getDagPath(0, dagp);

    status = MFnInstancer(dagp).allInstances(allPaths, allMatrices,
                                             pathStartIndices, pathIndices);
    if (status != MS::kSuccess) {
      MGlobal::displayWarning("[ExocortexAlembic] Instancer error: " +
                              status.errorString());
      return false;
    }
  }

  names.resize(allPaths.length());
  for (int i = 0; i < (int)allPaths.length(); ++i) {
    std::string nm = allPaths[i].fullPathName().asChar();
    size_t pos = nm.find("|");
    while (pos != std::string::npos) {
      nm[pos] = '/';
      pos = nm.find("|", pos);
    }
    names[i] = nm;
  }
  mInstanceNamesProperty.set(Abc::StringArraySample(names));
  return true;
}

bool AlembicPoints::sampleInstanceProperties(
    std::vector<Abc::Quatf> angularVel, std::vector<Abc::Quatf> orientation,
    std::vector<Abc::uint16_t> shapeId, std::vector<Abc::uint16_t> shapeType,
    std::vector<Abc::float32_t> shapeTime)
{
  MMatrixArray allMatrices;
  MIntArray pathIndices;
  MIntArray pathStartIndices;
  int nbParticles;
  {
    MDagPathArray allPaths;

    MSelectionList sl;
    sl.add(instName);

    MDagPath dagp;
    sl.getDagPath(0, dagp);

    MFnInstancer inst(dagp);
    inst.allInstances(allPaths, allMatrices, pathStartIndices, pathIndices);
    nbParticles = inst.particleCount();
  }

  // resize vectors!
  angularVel.resize(nbParticles);
  orientation.resize(nbParticles);
  shapeId.resize(nbParticles);
  shapeType.resize(nbParticles, 7);
  // shapeTime.resize(nbParticles, mNumSamples);

  float matrix_data[4][4];
  for (int i = 0; i < nbParticles; ++i) {
    angularVel[i] = Abc::Quatf();  // don't know how to access this one yet!
    allMatrices[i].get(matrix_data);
    orientation[i] = Imath::extractQuat(Imath::M44f(matrix_data));
    shapeId[i] =
        pathIndices[pathStartIndices[i]];  // only keep the first one...
  }

  mAngularVelocityProperty.set(Abc::QuatfArraySample(angularVel));
  mOrientationProperty.set(Abc::QuatfArraySample(orientation));
  mShapeInstanceIdProperty.set(Abc::UInt16ArraySample(shapeId));
  // mShapeTimeProperty.set(Abc::FloatArraySample(shapeTime));
  mShapeTypeProperty.set(Abc::UInt16ArraySample(shapeType));
  return true;
}

AlembicPoints::AlembicPoints(SceneNodePtr eNode, AlembicWriteJob *in_Job,
                             Abc::OObject oParent)
    : AlembicObject(eNode, in_Job, oParent), hasInstancer(false)
{
  const bool animTS = (GetJob()->GetAnimatedTs() > 0);
  mObject = AbcG::OPoints(GetMyParent(), eNode->name, animTS);
  mSchema = mObject.getSchema();

  Abc::OCompoundProperty arbGeomParam = mSchema.getArbGeomParams();
  mAgeProperty = Abc::OFloatArrayProperty(arbGeomParam, ".age",
                                          mSchema.getMetaData(), animTS);
  mMassProperty = Abc::OFloatArrayProperty(arbGeomParam, ".mass",
                                           mSchema.getMetaData(), animTS);
  mColorProperty = Abc::OC4fArrayProperty(arbGeomParam, ".color",
                                          mSchema.getMetaData(), animTS);

  mAngularVelocityProperty = Abc::OQuatfArrayProperty(
      arbGeomParam, ".angularvelocity", mSchema.getMetaData(), animTS);
  mInstanceNamesProperty = Abc::OStringArrayProperty(
      arbGeomParam, ".instancenames", mSchema.getMetaData(), animTS);
  mOrientationProperty = Abc::OQuatfArrayProperty(
      arbGeomParam, ".orientation", mSchema.getMetaData(), animTS);
  mScaleProperty = Abc::OV3fArrayProperty(arbGeomParam, ".scale",
                                          mSchema.getMetaData(), animTS);
  mShapeInstanceIdProperty = Abc::OUInt16ArrayProperty(
      arbGeomParam, ".shapeinstanceid", mSchema.getMetaData(), animTS);
  // mShapeTimeProperty       = Abc::OFloatArrayProperty  (arbGeomParam,
  // ".shapetime", mSchema.getMetaData(), animTS );
  mShapeTypeProperty = Abc::OUInt16ArrayProperty(arbGeomParam, ".shapetype",
                                                 mSchema.getMetaData(), animTS);

  MStringArray instancers;
  MGlobal::executeCommand(
      ("listConnections -t instancer " + eNode->name).c_str(), instancers);
  switch (instancers.length()) {
    case 0:
      break;
    case 1:
      hasInstancer = true;
      instName = instancers[0];
      break;
    default:
      MGlobal::displayWarning(
          ("[ExocortexAlembic] More than one instancer associated to " +
           eNode->name + ", can only use one.\nParticle instancing ignore.")
              .c_str());
      break;
  }
}

AlembicPoints::~AlembicPoints()
{
  mObject.reset();
  mSchema.reset();
}

MStatus AlembicPoints::Save(double time, unsigned int timeIndex,
    bool isFirstFrame)
{
  ESS_PROFILE_SCOPE("AlembicPoints::Save");
  // access the geometry
  MFnParticleSystem node(GetRef());

  // save the metadata
  SaveMetaData(this);

  // save the attributes
  if (isFirstFrame) {
    Abc::OCompoundProperty cp;
    Abc::OCompoundProperty up;
    if (AttributesWriter::hasAnyAttr(node, *GetJob())) {
      cp = mSchema.getArbGeomParams();
      up = mSchema.getUserProperties();
    }

    mAttrs = AttributesWriterPtr(
        new AttributesWriter(cp, up, GetMyParent(), node, timeIndex, *GetJob())
        );
  }
  else {
    mAttrs->write();
  }

  // prepare the bounding box
  Abc::Box3d bbox;

  // access the points
  MVectorArray vectors;
  node.position(vectors);

  // check if we have the global cache option
  const bool globalCache =
      GetJob()->GetOption(L"exportInGlobalSpace").asInt() > 0;
  Abc::M44f globalXfo;
  if (globalCache) {
    globalXfo = GetGlobalMatrix(GetRef());
  }

  // instance names, scale,
  std::vector<Abc::V3f> scales;
  std::vector<std::string> instanceNames;
  if (hasInstancer && mNumSamples == 0) {
    scales.push_back(Abc::V3f(1.0f, 1.0f, 1.0f));
    mScaleProperty.set(Abc::V3fArraySample(scales));

    listIntanceNames(instanceNames);
  }

  // push the positions to the bbox
  size_t particleCount = vectors.length();
  std::vector<Abc::V3f> posVec(particleCount);
  for (unsigned int i = 0; i < vectors.length(); i++) {
    const MVector &out = vectors[i];
    Abc::V3f &in = posVec[i];
    in.x = (float)out.x;
    in.y = (float)out.y;
    in.z = (float)out.z;
    if (globalCache) {
      globalXfo.multVecMatrix(in, in);
    }
    bbox.extendBy(in);
  }
  vectors.clear();

  // get the velocities
  node.velocity(vectors);
  std::vector<Abc::V3f> velVec(particleCount);
  for (unsigned int i = 0; i < vectors.length(); i++) {
    const MVector &out = vectors[i];
    Abc::V3f &in = velVec[i];
    in.x = (float)out.x;
    in.y = (float)out.y;
    in.z = (float)out.z;
    if (globalCache) {
      globalXfo.multDirMatrix(in, in);
    }
  }
  vectors.clear();

  // get the widths
  MDoubleArray doubles;
  node.radius(doubles);
  std::vector<float> widthVec(particleCount);
  for (unsigned int i = 0; i < doubles.length(); i++) {
    widthVec[i] = (float)doubles[i];
  }
  doubles.clear();

  // get the ids
  std::vector<Abc::uint64_t> idVec(particleCount);
  MIntArray ints;
  node.particleIds(ints);
  for (unsigned int i = 0; i < ints.length(); i++) {
    idVec[i] = (Abc::uint64_t)ints[i];
  }
  ints.clear();

  // get the age
  node.age(doubles);
  std::vector<float> ageVec(particleCount);
  for (unsigned int i = 0; i < doubles.length(); i++) {
    ageVec[i] = (float)doubles[i];
  }
  doubles.clear();

  // get the mass
  node.mass(doubles);
  std::vector<float> massVec(particleCount);
  for (unsigned int i = 0; i < doubles.length(); i++) {
    massVec[i] = (float)doubles[i];
  }
  doubles.clear();

  // get the color
  std::vector<Abc::C4f> colorVec;
  if (node.hasOpacity() || node.hasRgb()) {
    colorVec.resize(particleCount);
    node.rgb(vectors);
    node.opacity(doubles);
    for (unsigned int i = 0; i < doubles.length(); i++) {
      const MVector &out = vectors[i];
      Imath::C4f &in = colorVec[i];
      in.r = (float)out.x;
      in.g = (float)out.y;
      in.b = (float)out.z;
      in.a = (float)doubles[i];
    }
    vectors.clear();
    doubles.clear();
  }

  mSample.setSelfBounds(bbox);

  // setup the sample
  mSample.setPositions(Abc::P3fArraySample(posVec));
  mSample.setVelocities(Abc::V3fArraySample(velVec));
  mSample.setWidths(AbcG::OFloatGeomParam::Sample(
      Abc::FloatArraySample(widthVec), AbcG::kVertexScope));
  mSample.setIds(Abc::UInt64ArraySample(idVec));
  mAgeProperty.set(Abc::FloatArraySample(ageVec));
  mMassProperty.set(Abc::FloatArraySample(massVec));
  mColorProperty.set(Abc::C4fArraySample(colorVec));

  //--- instancing!!
  std::vector<Abc::Quatf> angularVel, orientation;
  std::vector<Abc::uint16_t> shapeId, shapeType;
  std::vector<Abc::float32_t> shapeTime;
  if (hasInstancer)
    sampleInstanceProperties(angularVel, orientation, shapeId, shapeType,
                             shapeTime);

  // save the sample
  mSchema.set(mSample);
  mNumSamples++;

  return MStatus::kSuccess;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// import
template <typename MDataType>
class MDataBasePropertyManager;
template <>
class MDataBasePropertyManager<MDoubleArray> : public BasePropertyManager {
 public:
  MDoubleArray data;
  bool valid;

  MDataBasePropertyManager(const std::string &name,
                           const Abc::ICompoundProperty &cmp)
      : BasePropertyManager(name, cmp), valid(false), data()
  {
  }

  void readFromParticle(const MFnParticleSystem &part)
  {
    MStatus status;
    part.getPerParticleAttribute(MString(attrName.c_str()), data, &status);
    if (status != MS::kSuccess)
      MGlobal::displayError("readFromParticle w/ " + MString(attrName.c_str()) +
                            " --> " + status.errorString());
  }
  virtual void readFromAbc(Alembic::AbcCoreAbstract::index_t floorIndex,
                           const unsigned int particleCount) = 0;
  void setParticleProperty(MFnParticleSystem &part)
  {
    if (!valid) {
      for (int i = 0; i < (int)data.length(); ++i) {
        data[i] = double();
      }
    }
    MStatus status;
    part.setPerParticleAttribute(MString(attrName.c_str()), data, &status);
    if (status != MS::kSuccess)
      MGlobal::displayError("setParticleProperty w/ " +
                            MString(attrName.c_str()) + " --> " +
                            status.errorString());
  }
  void initPerParticle(const MString &partName)
  {
    MGlobal::executePythonCommand("cmds.addAttr(\"" + partName + "\", ln=\"" +
                                  MString(attrName.c_str()) +
                                  "\", dt=\"doubleArray\")");
  }
};
template <>
class MDataBasePropertyManager<MVectorArray> : public BasePropertyManager {
 public:
  MVectorArray data;
  bool valid;

  MDataBasePropertyManager(const std::string &name,
                           const Abc::ICompoundProperty &cmp)
      : BasePropertyManager(name, cmp), valid(false), data()
  {
  }

  void readFromParticle(const MFnParticleSystem &part)
  {
    MStatus status;
    part.getPerParticleAttribute(MString(attrName.c_str()), data, &status);
    if (status != MS::kSuccess)
      MGlobal::displayError("readFromParticle w/ " + MString(attrName.c_str()) +
                            " --> " + status.errorString());
  }
  virtual void readFromAbc(Alembic::AbcCoreAbstract::index_t floorIndex,
                           const unsigned int particleCount) = 0;
  void setParticleProperty(MFnParticleSystem &part)
  {
    if (!valid) {
      for (int i = 0; i < (int)data.length(); ++i) {
        data[i] = MVector();
      }
    }
    MStatus status;
    part.setPerParticleAttribute(MString(attrName.c_str()), data, &status);
    if (status != MS::kSuccess)
      MGlobal::displayError("setParticleProperty w/ " +
                            MString(attrName.c_str()) + " --> " +
                            status.errorString());
  }
  void initPerParticle(const MString &partName)
  {
    MGlobal::executePythonCommand("cmds.addAttr(\"" + partName + "\", ln=\"" +
                                  MString(attrName.c_str()) +
                                  "\", dt=\"vectorArray\")");
  }
};

template <typename IProp>
class SingleValue : public MDataBasePropertyManager<MDoubleArray> {
  typedef typename IProp::sample_type sam_type;
  typedef MDataBasePropertyManager<MDoubleArray> base_type;

 public:
  SingleValue(const std::string &name, const Abc::ICompoundProperty &cmp)
      : base_type(name, cmp)
  {
  }

  void readFromAbc(Alembic::AbcCoreAbstract::index_t floorIndex,
                   const unsigned int particleCount)
  {
    valid = false;
    const IProp prop = IProp(comp, propName);
    if (!prop.valid() || prop.getNumSamples() <= 0) {
      return;
    }
    std::shared_ptr<sam_type> samples = prop.getValue(floorIndex);
    const size_t sam_size = samples->size();
    if (sam_size == 0 || sam_size != 1 && sam_size != particleCount) {
      return;
    }

    data.setLength(particleCount);
    valid = true;
    if (sam_size == 1) {
      const double single = (double)samples->get()[0];
      for (unsigned int i = 0; i < particleCount; ++i) {
        data[i] = single;
      }
    }
    else
      for (unsigned int i = 0; i < particleCount; ++i) {
        data[i] = (double)samples->get()[i];
      }
  }
};
template <typename IProp>
class PairValue : public MDataBasePropertyManager<MVectorArray> {
  typedef typename IProp::sample_type sam_type;
  typedef typename IProp::sample_type::value_type Ivalue_type;
  typedef MDataBasePropertyManager<MVectorArray> base_type;

 public:
  PairValue(const std::string &name, const Abc::ICompoundProperty &cmp)
      : base_type(name, cmp)
  {
  }

  void readFromAbc(Alembic::AbcCoreAbstract::index_t floorIndex,
                   const unsigned int particleCount)
  {
    valid = false;
    data.setLength(particleCount);
    const IProp prop = IProp(comp, propName);
    if (!prop.valid() || prop.getNumSamples() <= 0) {
      return;
    }
    std::shared_ptr<sam_type> samples = prop.getValue(floorIndex);

    const size_t sam_size = samples->size();
    if (sam_size == 0 || sam_size != 1 && sam_size != particleCount) {
      return;
    }

    valid = true;
    if (samples->size() == 1) {
      const Ivalue_type &defRead = samples->get()[0];
      const MVector def(defRead.x, defRead.y, 0.0);
      for (unsigned int i = 0; i < particleCount; ++i) {
        data[i] = def;
      }
    }
    else
      for (unsigned int i = 0; i < particleCount; ++i) {
        const Ivalue_type &defRead = samples->get()[i];
        data[i] = MVector(defRead.x, defRead.y, 0.0);
      }
  }
};
template <typename IProp>
class TripleValue : public MDataBasePropertyManager<MVectorArray> {
  typedef typename IProp::sample_type sam_type;
  typedef typename IProp::sample_type::value_type Ivalue_type;
  typedef MDataBasePropertyManager<MVectorArray> base_type;

 public:
  TripleValue(const std::string &name, const Abc::ICompoundProperty &cmp)
      : base_type(name, cmp)
  {
  }

  void readFromAbc(Alembic::AbcCoreAbstract::index_t floorIndex,
                   const unsigned int particleCount)
  {
    valid = false;
    data.setLength(particleCount);
    const IProp prop = IProp(comp, propName);
    if (!prop.valid() || prop.getNumSamples() <= 0) {
      return;
    }
    std::shared_ptr<sam_type> samples = prop.getValue(floorIndex);

    const size_t sam_size = samples->size();
    if (sam_size == 0 || sam_size != 1 && sam_size != particleCount) {
      return;
    }

    valid = true;
    if (samples->size() == 1) {
      const Ivalue_type &defRead = samples->get()[0];
      const MVector def(defRead.x, defRead.y, defRead.z);
      for (unsigned int i = 0; i < particleCount; ++i) {
        data[i] = def;
      }
    }
    else
      for (unsigned int i = 0; i < particleCount; ++i) {
        const Ivalue_type &defRead = samples->get()[i];
        data[i] = MVector(defRead.x, defRead.y, defRead.z);
      }
  }
};

ArbGeomProperties::ArbGeomProperties(const Abc::ICompoundProperty &comp)
    : valid(false)
{
  this->constructData(comp);
}

void ArbGeomProperties::constructData(const Abc::ICompoundProperty &comp)
{
  std::stringstream swarning;
  const int nb_prop = (int)comp.getNumProperties();
  for (int i = 0; i < nb_prop; ++i) {
    const Abc::AbcA::PropertyHeader &phead = comp.getPropertyHeader(i);
    const std::string name = phead.getName();
    if (name[0] == '.' &&
        (name == ".color" || name == ".age" || name == ".mass" ||
         name == ".shapeinstanceid" || name == ".orientation" ||
         name == ".angularvelocity")) {
      continue;
    }

    if (!phead.isArray()) {
      swarning << "Ignoring " << name << " because it's not an array!\n";
      continue;
    }

    BasePropertyManagerPtr bpmptr;

    const int extent = phead.getDataType().getExtent();
    switch (phead.getDataType().getPod()) {
      //! Rejects
      case Abc::kStringPOD:
      case Abc::kWstringPOD:
      case Abc::kNumPlainOldDataTypes:
      case Abc::kUnknownPOD: {
        swarning << "Ignoring " << name
                 << " because its type is not a valid for PerParticle!\n";
        continue;
      } break;

      //! Boolean
      case Abc::kBooleanPOD:
        bpmptr.reset(new SingleValue<Abc::IBoolArrayProperty>(name, comp));
        break;

      //! Char/UChar
      case Abc::kUint8POD:
        bpmptr.reset(new SingleValue<Abc::IUcharArrayProperty>(name, comp));
        break;
      case Abc::kInt8POD:
        bpmptr.reset(new SingleValue<Abc::ICharArrayProperty>(name, comp));
        break;

      //! Short/UShort
      case Abc::kUint16POD:
        bpmptr.reset(new SingleValue<Abc::IUInt16ArrayProperty>(name, comp));
        break;
      case Abc::kInt16POD:
        switch (extent) {
          case 1:
            bpmptr.reset(new SingleValue<Abc::IInt16ArrayProperty>(name, comp));
            break;
          case 2:
            bpmptr.reset(new PairValue<Abc::IV2sArrayProperty>(name, comp));
            break;
          case 3:
            bpmptr.reset(new TripleValue<Abc::IV3sArrayProperty>(name, comp));
            break;
          default:
            break;
        };
        break;

      //! Int/UInt
      case Abc::kUint32POD:
        bpmptr.reset(new SingleValue<Abc::IUInt32ArrayProperty>(name, comp));
        break;
      case Abc::kInt32POD:
        switch (extent) {
          case 1:
            bpmptr.reset(new SingleValue<Abc::IInt32ArrayProperty>(name, comp));
            break;
          case 2:
            bpmptr.reset(new PairValue<Abc::IV2iArrayProperty>(name, comp));
            break;
          case 3:
            bpmptr.reset(new TripleValue<Abc::IV3iArrayProperty>(name, comp));
            break;
          default:
            break;
        };
        break;

      //! Long/ULong
      case Abc::kUint64POD:
        bpmptr.reset(new SingleValue<Abc::IUInt64ArrayProperty>(name, comp));
        break;
      case Abc::kInt64POD:
        bpmptr.reset(new SingleValue<Abc::IInt64ArrayProperty>(name, comp));
        break;

      //! Half/Float/Double
      case Abc::kFloat16POD:
        bpmptr.reset(new SingleValue<Abc::IHalfArrayProperty>(name, comp));
        break;
      case Abc::kFloat32POD:
        switch (extent) {
          case 1:
            bpmptr.reset(new SingleValue<Abc::IFloatArrayProperty>(name, comp));
            break;
          case 2:
            bpmptr.reset(new PairValue<Abc::IV2fArrayProperty>(name, comp));
            break;
          case 3:
            bpmptr.reset(new TripleValue<Abc::IV3fArrayProperty>(name, comp));
            break;
          default:
            break;
        };
        break;
      case Abc::kFloat64POD:
        switch (extent) {
          case 1:
            bpmptr.reset(
                new SingleValue<Abc::IDoubleArrayProperty>(name, comp));
            break;
          case 2:
            bpmptr.reset(new PairValue<Abc::IV2dArrayProperty>(name, comp));
            break;
          case 3:
            bpmptr.reset(new TripleValue<Abc::IV3dArrayProperty>(name, comp));
            break;
          default:
            break;
        };
        break;

      default:
        break;
    }
    if (bpmptr.get()) {
      baseProperties.push_back(bpmptr);
    }
    else {
      swarning << "Ignoring " << name << ", couldn't find the proper type!\n";
    }
  }

  valid = true;
  const std::string warning = swarning.str();
  if (warning.length()) {
    MGlobal::displayWarning(warning.c_str());
  }
}
void ArbGeomProperties::initPerParticle(const MString &partName)
{
  MGlobal::executePythonCommand("import maya.cmds as cmds");
  for (std::vector<BasePropertyManagerPtr>::iterator
           beg = baseProperties.begin(),
           end = baseProperties.end();
       beg != end; ++beg) {
    (*beg)->initPerParticle(partName);
  }
}
void ArbGeomProperties::readFromParticle(const MFnParticleSystem &part)
{
  for (std::vector<BasePropertyManagerPtr>::iterator
           beg = baseProperties.begin(),
           end = baseProperties.end();
       beg != end; ++beg) {
    (*beg)->readFromParticle(part);
  }
}
void ArbGeomProperties::readFromAbc(
    Alembic::AbcCoreAbstract::index_t floorIndex,
    const unsigned int particleCount)
{
  for (std::vector<BasePropertyManagerPtr>::iterator
           beg = baseProperties.begin(),
           end = baseProperties.end();
       beg != end; ++beg) {
    (*beg)->readFromAbc(floorIndex, particleCount);
  }
}
void ArbGeomProperties::setParticleProperty(MFnParticleSystem &part)
{
  for (std::vector<BasePropertyManagerPtr>::iterator
           beg = baseProperties.begin(),
           end = baseProperties.end();
       beg != end; ++beg) {
    (*beg)->setParticleProperty(part);
  }
}

static AlembicPointsNodeList alembicPointsNodeList;

///////////////// AlembicPointsNode

void AlembicPointsNode::PostConstructor(void)
{
  alembicPointsNodeList.push_front(this);
  listPosition = alembicPointsNodeList.begin();
}

void AlembicPointsNode::PreDestruction()
{
  for (AlembicPointsNodeListIter beg = alembicPointsNodeList.begin();
       beg != alembicPointsNodeList.end(); ++beg) {
    if (beg == listPosition) {
      alembicPointsNodeList.erase(listPosition);
      break;
    }
  }

  mSchema.reset();
  delRefArchive(mFileName);
  mFileName.clear();
}

AlembicPointsNode::AlembicPointsNode()
    : mLastInputTime(-1.0)  //, arbGeomProperties(0)
{
  PostConstructor();
}

AlembicPointsNode::~AlembicPointsNode(void)
{
  PreDestruction();
  // if (arbGeomProperties)
  //  delete arbGeomProperties;
}

MObject AlembicPointsNode::mTimeAttr;
MObject AlembicPointsNode::mFileNameAttr;
MObject AlembicPointsNode::mIdentifierAttr;

MObject AlembicPointsNode::mGeomParamsList;
MObject AlembicPointsNode::mUserAttrsList;

MStatus AlembicPointsNode::initialize()
{
  MStatus status;

  MFnUnitAttribute uAttr;
  MFnTypedAttribute tAttr;
  MFnNumericAttribute nAttr;
  MFnGenericAttribute gAttr;
  MFnStringData emptyStringData;
  MObject emptyStringObject = emptyStringData.create("");

  // input time
  mTimeAttr = uAttr.create("inTime", "tm", MFnUnitAttribute::kTime, 0.0);
  status = uAttr.setStorable(true);
  status = uAttr.setKeyable(true);
  status = addAttribute(mTimeAttr);

  // input file name
  mFileNameAttr =
      tAttr.create("fileName", "fn", MFnData::kString, emptyStringObject);
  status = tAttr.setStorable(true);
  status = tAttr.setUsedAsFilename(true);
  status = tAttr.setKeyable(false);
  status = addAttribute(mFileNameAttr);

  // input identifier
  mIdentifierAttr =
      tAttr.create("identifier", "if", MFnData::kString, emptyStringObject);
  status = tAttr.setStorable(true);
  status = tAttr.setKeyable(false);
  status = addAttribute(mIdentifierAttr);

  // output for list of ArbGeomParams
  mGeomParamsList = tAttr.create("ExocortexAlembic_GeomParams", "exo_gp",
      MFnData::kString, emptyStringObject);
  status = tAttr.setStorable(true);
  status = tAttr.setKeyable(false);
  status = tAttr.setHidden(false);
  status = tAttr.setInternal(true);
  status = addAttribute(mGeomParamsList);

  // output for list of UserAttributes
  mUserAttrsList = tAttr.create("ExocortexAlembic_UserAttributes", "exo_ua",
      MFnData::kString, emptyStringObject);
  status = tAttr.setStorable(true);
  status = tAttr.setKeyable(false);
  status = tAttr.setHidden(false);
  status = tAttr.setInternal(true);
  status = addAttribute(mUserAttrsList);

  // create a mapping
  status = attributeAffects(mTimeAttr, mOutput);
  status = attributeAffects(mFileNameAttr, mOutput);
  status = attributeAffects(mIdentifierAttr, mOutput);

  return status;
}

MStatus AlembicPointsNode::init(const MString &fileName,
                                const MString &identifier)
{
  if (fileName != mFileName || identifier != mIdentifier) {
    mSchema.reset();
    if (fileName != mFileName) {
      delRefArchive(mFileName);
      mFileName = fileName;
      addRefArchive(mFileName);
    }
    mIdentifier = identifier;

    // get the object from the archive
    Abc::IObject iObj = getObjectFromArchive(mFileName, identifier);
    if (!iObj.valid()) {
      MGlobal::displayWarning("[ExocortexAlembic] Identifier '" + identifier +
                              "' not found in archive '" + mFileName + "'.");
      return MStatus::kFailure;
    }
    obj = AbcG::IPoints(iObj, Abc::kWrapExisting);
    if (!obj.valid()) {
      MGlobal::displayWarning("[ExocortexAlembic] Identifier '" + identifier +
                              "' in archive '" + mFileName +
                              "' is not a Points.");
      return MStatus::kFailure;
    }
    mSchema = obj.getSchema();

    if (!mSchema.valid()) {
      return MStatus::kFailure;
    }

    // if (arbGeomProperties)
    //	delete arbGeomProperties;
    // arbGeomProperties = new ArbGeomProperties(mSchema.getArbGeomParams());
  }
  return MS::kSuccess;
}
MStatus AlembicPointsNode::initPerParticles(const MString &partName)
{
  // arbGeomProperties->initPerParticle(partName);
  return MS::kSuccess;
}

static MVector quaternionToVector(const Abc::Quatf &qf,
                                  const Abc::Quatf &angVel)
{
  const Abc::Quatf qfVel = (angVel.r != 0.0f) ? (angVel * qf).normalize() : qf;

  Imath::V3f vec;
  extractEulerXYZ(qfVel.toMatrix44(), vec);
  return MVector(vec.x, vec.y, vec.z);
}

static bool useAngularVelocity(Abc::QuatfArraySamplePtr &velPtr,
                               const AbcG::IPoints &obj,
                               Alembic::AbcCoreAbstract::index_t floorIndex)
{
  Abc::IQuatfArrayProperty velProp;
  if (getArbGeomParamPropertyAlembic(obj, "angularvelocity", velProp)) {
    velPtr = velProp.getValue(floorIndex);
    return (velPtr != NULL && velPtr->size() != 0);
  }
  return false;
}

MStatus AlembicPointsNode::compute(const MPlug &plug, MDataBlock &dataBlock)
{
  ESS_PROFILE_SCOPE("AlembicPointsNode::compute");
  if (!(plug == mOutput)) {
    return (MS::kUnknownParameter);
  }

  MStatus status;

  // from the maya api examples (ownerEmitter.cpp)
  const int multiIndex = plug.logicalIndex(&status);
  MArrayDataHandle hOutArray = dataBlock.outputArrayValue(mOutput, &status);
  MArrayDataBuilder bOutArray = hOutArray.builder(&status);
  MDataHandle hOut = bOutArray.addElement(multiIndex, &status);
  MFnArrayAttrsData fnOutput;
  MObject dOutput = fnOutput.create(&status);

  MPlugArray connectionArray;
  plug.connectedTo(connectionArray, false, true, &status);
  if (connectionArray.length() == 0) {
    return status;
  }

  MPlug particleShapeOutPlug = connectionArray[0];
  MObject particleShapeNode = particleShapeOutPlug.node(&status);
  MFnParticleSystem part(particleShapeNode, &status);
  if (status != MS::kSuccess) {
    return status;
  }

  // update the frame number to be imported
  const double inputTime =
      dataBlock.inputValue(mTimeAttr).asTime().as(MTime::kSeconds);
  const MString &fileName = dataBlock.inputValue(mFileNameAttr).asString();
  const MString &identifier = dataBlock.inputValue(mIdentifierAttr).asString();

  // check if we have the file
  if (!(status = init(fileName, identifier))) {
    return status;
  }
  else {
    MFnDependencyNode depNode(particleShapeNode);
    // initPerParticles(depNode.name());
  }

  // get the sample
  SampleInfo sampleInfo = getSampleInfo(inputTime, mSchema.getTimeSampling(),
                                        mSchema.getNumSamples());

  // check if we have to do this at all
  if (mLastInputTime == inputTime &&
      mLastSampleInfo.floorIndex == sampleInfo.floorIndex &&
      mLastSampleInfo.ceilIndex == sampleInfo.ceilIndex) {
    return MStatus::kSuccess;
  }

  mLastInputTime = inputTime;
  mLastSampleInfo = sampleInfo;

  {
    ESS_PROFILE_SCOPE("AlembicPointsNode::compute readProps");
    Alembic::Abc::ICompoundProperty arbProp = mSchema.getArbGeomParams();
    Alembic::Abc::ICompoundProperty userProp = mSchema.getUserProperties();
    readProps(inputTime, arbProp, dataBlock, thisMObject());
    readProps(inputTime, userProp, dataBlock, thisMObject());

    // Set all plugs as clean
    // Even if one of them failed to get set,
    // trying again in this frame isn't going to help
    for (unsigned int i = 0; i < mGeomParamPlugs.length(); i++) {
      dataBlock.outputValue(mGeomParamPlugs[i]).setClean();
    }

    for (unsigned int i = 0; i < mUserAttrPlugs.length(); i++) {
      dataBlock.outputValue(mUserAttrPlugs[i]).setClean();
    }
  }

  // access the points values
  AbcG::IPointsSchema::Sample sample;
  mSchema.get(sample, sampleInfo.floorIndex);

  // get all of the data from alembic
  Abc::UInt64ArraySamplePtr sampleIds = sample.getIds();
  Abc::P3fArraySamplePtr samplePos = sample.getPositions();
  Abc::V3fArraySamplePtr sampleVel = sample.getVelocities();

  Abc::C4fArraySamplePtr sampleColor;
  {
    Abc::IC4fArrayProperty propColor;
    if (getArbGeomParamPropertyAlembic(obj, "color", propColor)) {
      sampleColor = propColor.getValue(sampleInfo.floorIndex);
    }
  }
  Abc::FloatArraySamplePtr sampleAge;
  {
    Abc::IFloatArrayProperty propAge;
    if (getArbGeomParamPropertyAlembic(obj, "age", propAge)) {
      sampleAge = propAge.getValue(sampleInfo.floorIndex);
    }
  }
  Abc::FloatArraySamplePtr sampleMass;
  {
    Abc::IFloatArrayProperty propMass;
    if (getArbGeomParamPropertyAlembic(obj, "mass", propMass)) {
      sampleMass = propMass.getValue(sampleInfo.floorIndex);
    }
  }
  Abc::UInt16ArraySamplePtr sampleShapeInstanceID;
  {
    Abc::IUInt16ArrayProperty propShapeInstanceID;
    if (getArbGeomParamPropertyAlembic(obj, "shapeinstanceid",
                                       propShapeInstanceID))
      sampleShapeInstanceID =
          propShapeInstanceID.getValue(sampleInfo.floorIndex);
  }
  Abc::QuatfArraySamplePtr sampleOrientation;
  {
    Abc::IQuatfArrayProperty propOrientation;
    if (getArbGeomParamPropertyAlembic(obj, "orientation", propOrientation)) {
      sampleOrientation = propOrientation.getValue(sampleInfo.floorIndex);
    }
  }

  // get the current values from the particle cloud
  MIntArray ids;
  part.particleIds(ids);
  MVectorArray positions;
  part.position(positions);
  MVectorArray velocities;
  part.velocity(velocities);
  MVectorArray rgbs;
  part.rgb(rgbs);
  MDoubleArray opacities;
  part.opacity(opacities);
  MDoubleArray ages;
  part.age(ages);
  MDoubleArray masses;
  part.mass(masses);
  MDoubleArray shapeInstId;
  part.getPerParticleAttribute("shapeInstanceIdPP", shapeInstId);
  MVectorArray orientationPP;
  part.getPerParticleAttribute("orientationPP", orientationPP, &status);
  if (status != MS::kSuccess)
    MGlobal::displayError("readFromParticle w/ orientationPP --> " +
                          status.errorString());

  // arbGeomProperties->readFromParticle(part);

  // check if this is a valid sample
  unsigned int particleCount = (unsigned int)samplePos->size();
  if (sampleIds->size() == 1) {
    if (sampleIds->get()[0] == (Abc::uint64_t)-1) {
      particleCount = 0;
    }
  }
  //*
  // ensure to have the right amount of particles
  if (positions.length() > particleCount) {
    part.setCount(particleCount);
  }
  else {
    const int deltaCount = particleCount - positions.length();
    MPointArray emitted(deltaCount);
    part.emit(emitted);
  }
  //*
  positions.setLength(particleCount);
  velocities.setLength(particleCount);
  rgbs.setLength(particleCount);
  opacities.setLength(particleCount);
  ages.setLength(particleCount);
  masses.setLength(particleCount);
  shapeInstId.setLength(particleCount);
  orientationPP.setLength(particleCount);

  // arbGeomProperties->readFromAbc(sampleInfo.floorIndex, particleCount);

  // if we need to emit new particles, do that now
  if (particleCount > 0) {
    const bool validVel = sampleVel && sampleVel->get();
    const bool validCol = sampleColor && sampleColor->get();
    const bool validAge = sampleAge && sampleAge->get();
    const bool validMas = sampleMass && sampleMass->get();
    const bool validSid = sampleShapeInstanceID && sampleShapeInstanceID->get();
    const bool validOri = sampleOrientation && sampleOrientation->get();
    const float timeAlpha = getTimeOffsetFromSchema(mSchema, sampleInfo);

    const bool velUseFirstSample = validVel && (sampleVel->size() == 1);
    const bool colUseFirstSample = validCol && (sampleColor->size() == 1);
    const bool ageUseFirstSample = validAge && (sampleAge->size() == 1);
    const bool masUseFirstSample = validMas && (sampleMass->size() == 1);
    const bool sidUseFirstSample =
        validSid && (sampleShapeInstanceID->size() == 1);
    const bool oriUseFirstSample = validOri && (sampleOrientation->size() == 1);
    for (unsigned int i = 0; i < particleCount; ++i) {
      //*
      const Alembic::Abc::V3f &pout = samplePos->get()[i];
      MVector &pin = positions[i];
      pin.x = pout.x;
      pin.y = pout.y;
      pin.z = pout.z;

      if (validVel) {
        const Alembic::Abc::V3f &out =
            sampleVel->get()[velUseFirstSample ? 0 : i];
        MVector &in = velocities[i];
        in.x = out.x;
        in.y = out.y;
        in.z = out.z;

        pin += in * timeAlpha;
      }

      if (validCol) {
        const Alembic::Abc::C4f &out =
            sampleColor->get()[colUseFirstSample ? 0 : i];
        MVector &in = rgbs[i];
        in.x = out.r;
        in.y = out.g;
        in.z = out.b;
        opacities[i] = out.a;
      }
      else {
        rgbs[i] = MVector(0.0, 0.0, 0.0);
        opacities[i] = 1.0;
      }

      ages[i] = validAge ? sampleAge->get()[ageUseFirstSample ? 0 : i] : 0.0;
      shapeInstId[i] =
          validSid ? sampleShapeInstanceID->get()[sidUseFirstSample ? 0 : i]
                   : 0.0;
      if (validMas) {
        const double mass = sampleMass->get()[masUseFirstSample ? 0 : i];
        masses[i] = mass > 0.0 ? mass : 1.0;
      }
      else {
        masses[i] = 1.0;
      }
      //*
    }

    // compute the right orientation with the angular velocity if necessary!
    //*
    if (validOri) {
      Abc::QuatfArraySamplePtr velPtr;
      const bool useAngVel =
          timeAlpha != 0.0f &&
          useAngularVelocity(velPtr, obj, sampleInfo.floorIndex);
      const bool angUseFirstSample = useAngVel && (velPtr->size() == 1);
      for (unsigned int i = 0; i < particleCount; ++i) {
        const Abc::Quatf angVel =
            (useAngVel ? velPtr->get()[angUseFirstSample ? 0 : i] * timeAlpha
                       : Abc::Quatf());
        orientationPP[i] = quaternionToVector(
            sampleOrientation->get()[oriUseFirstSample ? 0 : i], angVel);
      }
    }
    else {
      for (unsigned int i = 0; i < particleCount; ++i) {
        orientationPP[i] = MVector::zero;
      }
    }  //*
  }

  // take care of the remaining attributes
  part.setPerParticleAttribute("position", positions);
  part.setPerParticleAttribute("velocity", velocities);
  part.setPerParticleAttribute("rgbPP", rgbs);
  part.setPerParticleAttribute("opacityPP", opacities);
  part.setPerParticleAttribute("agePP", ages);
  part.setPerParticleAttribute("massPP", masses);
  part.setPerParticleAttribute("shapeInstanceIdPP", shapeInstId);
  part.setPerParticleAttribute("orientationPP", orientationPP);
  //*/
  // arbGeomProperties->setParticleProperty(part);

  hOut.set(dOutput);
  dataBlock.setClean(plug);

  return MStatus::kSuccess;
}

void AlembicPointsNode::instanceInitialize(void)
{
  {
    const MString getAttrCmd = "getAttr " + name();
    MString fileName, identifier;

    MGlobal::executeCommand(getAttrCmd + ".fileName", fileName);
    MGlobal::executeCommand(getAttrCmd + ".identifier", identifier);

    if (!init(fileName, identifier)) {
      return;
    }
  }

  {
    Abc::IUInt16ArrayProperty propShapeT;
    if (!getArbGeomParamPropertyAlembic(obj, "shapetype", propShapeT)) {
      return;
    }

    Abc::UInt16ArraySamplePtr sampleShapeT =
        propShapeT.getValue(propShapeT.getNumSamples() - 1);
    if (!sampleShapeT || sampleShapeT->size() <= 0 ||
        sampleShapeT->get()[0] != 7) {
      return;
    }
  }

  // check if there's instance names! If so, load the names!... if there's any!
  Abc::IStringArrayProperty propInstName;
  if (!getArbGeomParamPropertyAlembic(obj, "instancenames", propInstName)) {
    return;
  }

  Abc::StringArraySamplePtr instNames =
      propInstName.getValue(propInstName.getNumSamples() - 1);
  const int nbNames = (int)instNames->size();
  if (nbNames < 1 || (nbNames == 1 && instNames->get()[0].length() == 0)) {
    return;
  }

  MString addObjectCmd = " -addObject";
  {
    const MString _obj = " -object ";
    for (int i = 0; i < nbNames; ++i) {
      std::string res = instNames->get()[i];
      if (res.length() == 0) {
        continue;
      }

      size_t pos = res.find("/");
      int repl = 0;
      while (pos != std::string::npos) {
        ++repl;
        res[pos] = '|';
        pos = res.find("/", pos);
      }
      res = removeInvalidCharacter(res, true);

      if (repl == 1)  // repl == 1 means it might be just a shape... need this
                      // for backward compatibility!
      {
        MSelectionList sl;
        sl.add(res.substr(1).c_str());
        MDagPath dag;
        sl.getDagPath(0, dag);
        res = dag.fullPathName().asChar();
      }
      addObjectCmd += _obj + res.c_str();
    }
    addObjectCmd += " ";
  }

  // create the particle instancer and assign shapeInstanceIdPP as the object
  // index!
  MString particleShapeName, mInstancerName;
  MGlobal::executeCommand("string $___connectInfo[] = `connectionInfo -dfs " +
                              name() +
                              ".output[0]`;\nplugNode $___connectInfo[0];",
                          particleShapeName);

  MGlobal::executeCommand("particleInstancer " + particleShapeName,
                          mInstancerName);
  const MString partInstCmd = "particleInstancer -e -name " + mInstancerName;
  MGlobal::executeCommand(partInstCmd +
                          " -rotationUnits Radians -rotationOrder XYZ "
                          "-objectIndex shapeInstanceIdPP -rotation "
                          "orientationPP " +
                          particleShapeName);
  MGlobal::executeCommand(partInstCmd + addObjectCmd + particleShapeName);
}

// Cache the plug arrays for use in setDependentsDirty
bool AlembicPointsNode::setInternalValueInContext(const MPlug & plug,
    const MDataHandle & dataHandle,
    MDGContext &)
{
  if (plug == mGeomParamsList) {
    MString geomParamsStr = dataHandle.asString();
    getPlugArrayFromAttrList(geomParamsStr, thisMObject(), mGeomParamPlugs);
  }
  else if (plug == mUserAttrsList) {
    MString userAttrsStr = dataHandle.asString();
    getPlugArrayFromAttrList(userAttrsStr, thisMObject(), mUserAttrPlugs);
  }

  return false;
}

MStatus AlembicPointsNode::setDependentsDirty(const MPlug &plugBeingDirtied,
    MPlugArray &affectedPlugs)
{
  if (plugBeingDirtied == mFileNameAttr || plugBeingDirtied == mIdentifierAttr
      || plugBeingDirtied == mTimeAttr) {

    for (unsigned int i = 0; i < mGeomParamPlugs.length(); i++) {
      affectedPlugs.append(mGeomParamPlugs[i]);
    }

    for (unsigned int i = 0; i < mUserAttrPlugs.length(); i++) {
      affectedPlugs.append(mUserAttrPlugs[i]);
    }
  }

  return MStatus::kSuccess;
}

MStatus AlembicPostImportPoints(void)
{
  ESS_PROFILE_SCOPE("AlembicPostImportPoints");
  AlembicPointsNodeListIter beg = alembicPointsNodeList.begin(),
                            end = alembicPointsNodeList.end();
  for (; beg != end; ++beg) {
    (*beg)->instanceInitialize();
  }
  return MS::kSuccess;
}

AlembicPostImportPointsCommand::AlembicPostImportPointsCommand(void) {}
AlembicPostImportPointsCommand::~AlembicPostImportPointsCommand(void) {}
MStatus AlembicPostImportPointsCommand::doIt(const MArgList &args)
{
  return AlembicPostImportPoints();
}

MSyntax AlembicPostImportPointsCommand::createSyntax(void) { return MSyntax(); }
void *AlembicPostImportPointsCommand::creator(void)
{
  return new AlembicPostImportPointsCommand();
}
