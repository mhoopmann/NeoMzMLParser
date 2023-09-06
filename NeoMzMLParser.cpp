#include "NeoMzMLParser.h"

using namespace std;

// Static callback handlers
static void NeoMzMLParser_startElementCallback(void *data, const XML_Char *el, const XML_Char **attr) {
  ((NeoMzMLParser*)data)->startElement(el, attr);
}

static void NeoMzMLParser_endElementCallback(void *data, const XML_Char *el){
  ((NeoMzMLParser*)data)->endElement(el);
}

static void NeoMzMLParser_charactersCallback(void *data, const XML_Char *s, int len){
  ((NeoMzMLParser*)data)->characters(s, len);
}

NeoMzMLParser::NeoMzMLParser(){
  init();
}

NeoMzMLParser::~NeoMzMLParser(){
  if (xml_fptr != NULL) fclose(xml_fptr);
}

void NeoMzMLParser::characters(const XML_Char *s, int len) {
  char* st = new char[len + 1];

  switch (activeEl.back()){
  case mzBinary: 
    strncpy(st, s, len);
    st[len] = '\0';
    if (activeEl[activeEl.size() - 4] == mzSpectrum) {
      mzML.run.spectrumList.spectrum.back().binaryDataArrayList.back().binaryDataArray.back().binary.content += st;
    } else if (activeEl[activeEl.size() - 4] == mzChromatogram) {
      mzML.run.chromatogramList[0].chromatogram.back().binaryDataArrayList.binaryDataArray.back().binary.content += st;
    } else {
      cout << "ERROR: unhandled content of binary element." << endl;
      exit(1);
    }
    break;
  case mzFileChecksum:
    strncpy(st, s, len);
    st[len] = '\0';
    indexedmzML.fileChecksum.content += st;
    break;
  case mzIndexListOffset:
    strncpy(st, s, len);
    st[len] = '\0';
    indexedmzML.indexListOffset.content += st;
    break;
  case mzOffset:
    strncpy(st, s, len);
    st[len] = '\0';
    indexedmzML.indexList.index.back().offset.back().content += st;
    break;
  default:
    //cout << elements[activeEl.back()];
    //cout << " unprocessed characters: " << endl; // << s << endl;
    break;
  }

  delete [] st;
}

void NeoMzMLParser::createIndex(){
  bIndexed = true;
  indexedmzML.xmlns = "http://psi.hupo.org/ms/mzml";
  indexedmzML.xmlns_xsi = "http://www.w3.org/2001/XMLSchema-instance";
  indexedmzML.xsi_schemaLocation = "http://psi.hupo.org/ms/mzml http://psidev.info/files/ms/mzML/xsd/mzML1.1.2_idx.xsd";
  indexedmzML.mzML=&mzML;
  //always have a spectrumlist
  CnmzIndex i;
  i.name="spectrum";
  indexedmzML.indexList.index.push_back(i);
  mzML.run.spectrumList.spectrumIndex=&indexedmzML.indexList.index.back();
  indexedmzML.indexList.count++;
  //only index chromatogramlist if it exists
  if(!mzML.run.chromatogramList.empty()){
    i.name="chromatogram";
    indexedmzML.indexList.index.push_back(i);
    mzML.run.chromatogramList[0].chromatogramIndex = &indexedmzML.indexList.index.back();
    indexedmzML.indexList.count++;
  }
}

void NeoMzMLParser::endElement(const XML_Char *el) {

  if (elSkip != MZML_NUM_ELEMENTS){
    if (isElement(elements[elSkip].c_str(), el)) elSkip = MZML_NUM_ELEMENTS;
    else return;
  }
  //cout << "/" << el << endl; //for diagnostics

  string s;
  for (int i = 0; i<MZML_NUM_ELEMENTS; i++){
    if (isElement(elements[i].c_str(), el)){
      if (activeEl.back() != (mzMLElement)i) cout << "Error: unexpected end element: " << elements[i] << " should be " << elements[activeEl.back()] << endl;
      else activeEl.pop_back();
      break;
    }
  }

  if (isElement("chromatogram", el)) {
    if (bIterative) {
      bParseAbort = true;
      XML_StopParser(parser, false);
    }

  } else if (isElement("indexList", el)) {
    if (bParseIndexList) {
      bParseAbort = true;
      XML_StopParser(parser, false);
    }

  } else if (isElement("mzML", el)) {
    bParseAbort = true;
    XML_StopParser(parser, false);

  } else if (isElement("spectrum", el)) {
    if (bIterative) {
      bParseAbort = true;
      XML_StopParser(parser, false);
    }
  }

  
}

CnmzChromatogram* NeoMzMLParser::nextChromatogram(bool buffered){
  if (!bIterative) {
    cerr << "WARNING: NeoMzMLParser::nextChromatogram(): mzML file must be opened in iterative mode." << endl;
    return NULL;
  }
  if (!bIndexed) {
    cerr << "WARNING: NeoMzMLParser::nextChromatogram(): mzML file must be indexed to use this funciton." << endl;
    return NULL;
  }
  if (!buffered){
    if(!mzML.run.chromatogramList.empty()) mzML.run.chromatogramList[0].chromatogram.clear();
  }

  if(chromatogramIndexID==SIZE_MAX) return NULL;

  chromatIndex++;
  if (chromatIndex >= indexedmzML.indexList.index[chromatogramIndexID].offset.size()){
    return NULL;
  } else {
    parse(atoll(indexedmzML.indexList.index[chromatogramIndexID].offset[chromatIndex].content.c_str()));
  }
  return &mzML.run.chromatogramList[0].chromatogram.back();
}

CnmzSpectrum* NeoMzMLParser::nextSpectrum(bool buffered){
  if(!bIterative) {
    cerr << "WARNING: NeoMzMLParser::nextSpectrum(): mzML file must be opened in iterative mode." << endl;
    return NULL;
  }
  if(!bIndexed) {
    cerr << "WARNING: NeoMzMLParser::nextSpectrum(): mzML file must be indexed to use this funciton." << endl;
    return NULL;
  }
  if(!buffered){
    mzML.run.spectrumList.spectrum.clear();
  }

  scanIndex++;
  if(scanIndex>=indexedmzML.indexList.index[spectrumIndexID].offset.size()){
    return NULL;
  } else {
    parse(atoll(indexedmzML.indexList.index[spectrumIndexID].offset[scanIndex].content.c_str()));
  }
  return &mzML.run.spectrumList.spectrum.back();
}

void NeoMzMLParser::init() {
  xml_fptr=NULL;
  Wptr=NULL;
  bIndexed=false;
  bIterative=false;
  bParseIndexList=false;
  bParseAbort=true;
  WindexList=NULL;

  elements[mzActivation] = "activation"; 
  elements[mzAnalyzer] = "analyzer";
  elements[mzBinary] = "binary";
  elements[mzBinaryDataArray] = "binaryDataArray";
  elements[mzBinaryDataArrayList] = "binaryDataArrayList";
  elements[mzChromatogram] = "chromatogram";
  elements[mzChromatogramList] = "chromatogramList";
  elements[mzComponentList] = "componentList";
  elements[mzCv] = "cv";
  elements[mzCvList] = "cvList";
  elements[mzCvParam] = "cvParam";
  elements[mzDataProcessing] = "dataProcessing";
  elements[mzDataProcessingList] = "dataProcessingList";
  elements[mzDetector] = "detector";
  elements[mzFileChecksum] = "fileChecksum";
  elements[mzFileContent] = "fileContent";
  elements[mzFileDescription] = "fileDescription";
  elements[mzIndex] = "index";
  elements[mzIndexedmzML] = "indexedmzML";
  elements[mzIndexList] = "indexList";
  elements[mzIndexListOffset] = "indexListOffset";
  elements[mzInstrumentConfiguration] = "instrumentConfiguration";
  elements[mzInstrumentConfigurationList] = "instrumentConfigurationList";
  elements[mzIsolationWindow] = "isolationWindow";
  elements[mzMzML] = "mzML";
  elements[mzOffset] = "offset";
  elements[mzPrecursor] = "precursor";
  elements[mzPrecursorList] = "precursorList";
  elements[mzProcessingMethod] = "processingMethod";
  elements[mzReferenceableParamGroup] = "referenceableParamGroup";
  elements[mzReferenceableParamGroupList] = "referenceableParamGroupList";
  elements[mzReferenceableParamGroupRef] = "referenceableParamGroupRef";
  elements[mzRun] = "run";
  elements[mzSample] = "sample";
  elements[mzSampleList] = "sampleList";
  elements[mzScan] = "scan";
  elements[mzScanList] = "scanList";
  elements[mzScanWindow] = "scanWindow";
  elements[mzScanWindowList] = "scanWindowList";
  elements[mzSelectedIon] = "selectedIon";
  elements[mzSelectedIonList] = "selectedIonList";
  elements[mzSoftware] = "software";
  elements[mzSoftwareList] = "softwareList";
  elements[mzSoftwareRef] = "softwareRef";
  elements[mzSource] = "source";
  elements[mzSourceFile] = "sourceFile";
  elements[mzSourceFileList] = "sourceFileList";
  elements[mzSpectrum] = "spectrum";
  elements[mzSpectrumList] = "spectrumList";
  elements[mzUserParam] = "userParam";
}

void NeoMzMLParser::parse(f_off_nmz offset) {

  activeEl.clear();

  parser = XML_ParserCreate(NULL);
  XML_SetUserData(parser, this);
  XML_SetElementHandler(parser, NeoMzMLParser_startElementCallback, NeoMzMLParser_endElementCallback);
  XML_SetCharacterDataHandler(parser, NeoMzMLParser_charactersCallback);

  nmzfseek(xml_fptr, offset, SEEK_SET);
  bParseAbort=false;

  char buffer[16384];
  int readBytes = 0;
  bool success = true;

  while (success && (readBytes = (int)fread(buffer, 1, sizeof(buffer), xml_fptr)) != 0){
    success = (XML_Parse(parser, buffer, readBytes, false) != 0);
  }
  success = success && (XML_Parse(parser, buffer, 0, true) != 0);

  if (!success) {
    XML_Error error = XML_GetErrorCode(parser);

    if (bParseAbort && (error == XML_ERROR_ABORTED || error == XML_ERROR_FINISHED)) {
      //cout << "Aborted is ok" << endl;
    } else xmlError(error);
  }

  XML_ParserFree(parser);
}

void NeoMzMLParser::parseChromatogramList(){
  if (chromatogramIndexID == SIZE_MAX) return;
  char buffer[300];
  string s;
  size_t sz;
  f_off_nmz indexOffset = nmzatoi64(indexedmzML.indexList.index[chromatogramIndexID].offset[0].content.c_str());

  nmzfseek(xml_fptr, indexOffset - 300, SEEK_SET);
  sz = fread(buffer, 1, 300, xml_fptr);
  buffer[299]='\0';
  s = buffer;
  sz = s.find("<chromatogramList");
  if (sz == string::npos) {
    cout << "ERROR: Cannot iteratively find chromatogramList" << endl;
    exit(1);
  }
  parse(indexOffset-300 + sz);
}

void NeoMzMLParser::parseIndex(){

  char buffer[200];
  char* start;
  char* stop;
  size_t sz;

  nmzfseek(xml_fptr, -200, SEEK_END);
  sz = fread(buffer, 1, 200, xml_fptr);
  start = strstr(buffer, "<indexListOffset>");
  stop = strstr(buffer, "</indexListOffset>");

  if (start == NULL || stop == NULL) {
    cerr << "No index list offset found. File will not be read." << endl;
    exit(1);
  }

  char offset[64];
  int len = (int)(stop - start - 17);
  strncpy(offset, start + 17, len);
  offset[len] = '\0';
  f_off_nmz indexOffset = nmzatoi64(offset);

  if (indexOffset == 0){
    cerr << "No index list offset found. File will not be read." << endl;
    exit(1);
  } else {
    //cout << indexOffset << endl;
    bParseIndexList=true;
    parse(indexOffset);
    bParseIndexList=false;
  }

}

void NeoMzMLParser::processCvParam(CnmzCvParam& c){
  mzMLElement e;
  if (activeEl.size()>1) e = activeEl.at(activeEl.size() - 2);
  else {
    cerr << "cvParam outside container element" << endl;
    exit(1);
  }
  switch (e){
  case mzActivation:
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.back().activation.cvParam.push_back(c);
    break;
  case mzAnalyzer:
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.back().analyzer.back().cvParam.push_back(c);
    break;
  case mzBinaryDataArray:
    if(activeEl[activeEl.size()-4]==mzSpectrum){
      mzML.run.spectrumList.spectrum.back().binaryDataArrayList.back().binaryDataArray.back().cvParam.push_back(c);
    } else if (activeEl[activeEl.size() - 4] == mzChromatogram){
      mzML.run.chromatogramList.back().chromatogram.back().binaryDataArrayList.binaryDataArray.back().cvParam.push_back(c);
    } else {
      cout << "Error: cvParam for BinaryDataArray cannot be assigned." << endl;
      exit(1);
    }
    break;
  case mzChromatogram:
    mzML.run.chromatogramList.back().chromatogram.back().cvParam.push_back(c);
    break;
  case mzDetector:
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.back().detector.back().cvParam.push_back(c);
    break;
  case mzFileContent:
    mzML.fileDescription.fileContent.cvParam.push_back(c);
    break;
  case mzInstrumentConfiguration:
    mzML.instrumentConfigurationList.instrumentConfiguration.back().cvParam.push_back(c);
    break;
  case mzIsolationWindow:
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.back().isolationWindow.back().cvParam.push_back(c);
    break;
  case mzProcessingMethod:
    mzML.dataProcessingList.dataProcessing.back().processingMethod.back().cvParam.push_back(c);
    break;
  case mzReferenceableParamGroup:
    mzML.referencableParamGroupList.back().referenceableParamGroup.back().cvParam.push_back(c);
    break;
  case mzSample:
    mzML.sampleList.back().sample.back().cvParam.push_back(c);
    break;
  case mzScan:
    mzML.run.spectrumList.spectrum.back().scanList.back().scan.back().cvParam.push_back(c);
    break;
  case mzScanList:
    mzML.run.spectrumList.spectrum.back().scanList.back().cvParam.push_back(c);
    break;
  case mzScanWindow:
    mzML.run.spectrumList.spectrum.back().scanList.back().scan.back().scanWindowList.back().scanWindow.back().cvParam.push_back(c);
    break;
  case mzSelectedIon:
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.back().selectedIonList.back().selectedIon.back().cvParam.push_back(c);
    break;
  case mzSoftware:
    mzML.softwareList.software.back().cvParam.push_back(c);
    break;
  case mzSource:
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.back().source.back().cvParam.push_back(c);
    break;
  case mzSourceFile:
    mzML.fileDescription.sourceFileList.back().sourceFile.back().cvParam.push_back(c);
    break;
  case mzSpectrum:
    mzML.run.spectrumList.spectrum.back().cvParam.push_back(c);
    break;
  default:
    cerr << "cvParam associated with " << elements[e] << " not handled." << endl;
    break;
  }
}

void NeoMzMLParser::processUserParam(CnmzUserParam& c){
  mzMLElement e;
  if (activeEl.size()>1) e = activeEl.at(activeEl.size() - 2);
  else {
    cerr << "userParam outside container element" << endl;
    exit(1);
  }
  switch (e){
  case mzActivation:
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.back().activation.userParam.push_back(c);
    break;
  case mzAnalyzer:
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.back().analyzer.back().userParam.push_back(c);
    break;
  case mzDetector:
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.back().detector.back().userParam.push_back(c);
    break;
  case mzFileContent:
    mzML.fileDescription.fileContent.userParam.push_back(c);
    break;
  case mzIsolationWindow:
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.back().isolationWindow.back().userParam.push_back(c);
    break;
  case mzProcessingMethod:
    mzML.dataProcessingList.dataProcessing.back().processingMethod.back().userParam.push_back(c);
    break;
  case mzReferenceableParamGroup:
    mzML.referencableParamGroupList.back().referenceableParamGroup.back().userParam.push_back(c);
    break;
  case mzSample:
    mzML.sampleList.back().sample.back().userParam.push_back(c);
    break;
  case mzScan:
    mzML.run.spectrumList.spectrum.back().scanList.back().scan.back().userParam.push_back(c);
    break;
  case mzSoftware:
    mzML.softwareList.software.back().userParam.push_back(c);
    break;
  case mzSource:
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.back().source.back().userParam.push_back(c);
    break;
  case mzSourceFile:
    mzML.fileDescription.sourceFileList.back().sourceFile.back().userParam.push_back(c);
    break;
  case mzSpectrum:
    mzML.run.spectrumList.spectrum.back().userParam.push_back(c);
    break;
  default:
    cerr << "userParam associated with " << elements[e] << " not handled." << endl;
    break;
  }
}

void NeoMzMLParser::startElement(const XML_Char *el, const XML_Char **attr){
  
  if (elSkip != MZML_NUM_ELEMENTS) return;
  //cout << el << endl; //for diagnostics
  
  if (isElement("activation", el)){
    activeEl.push_back(mzActivation);
    CnmzActivation c;
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.back().activation=c;

  } else if(isElement("analyzer", el)){
    activeEl.push_back(mzAnalyzer);
    CnmzAnalyzer c;
    c.order = atoi(getAttrValue("order", attr));
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.back().analyzer.push_back(c);

  } else if (isElement("binary", el)){
    activeEl.push_back(mzBinary);
    CnmzBinary c;
    if (activeEl[activeEl.size() - 4] == mzSpectrum) {
      mzML.run.spectrumList.spectrum.back().binaryDataArrayList.back().binaryDataArray.back().binary=c;
    } else if (activeEl[activeEl.size() - 4] == mzChromatogram){
      mzML.run.chromatogramList[0].chromatogram.back().binaryDataArrayList.binaryDataArray.back().binary = c;
    } else {
      cout << "Error: unknown parent of binary: " << activeEl[activeEl.size() - 4] << endl;
      exit(1);
    }

  } else if (isElement("binaryDataArray", el)){
    activeEl.push_back(mzBinaryDataArray);
    CnmzBinaryDataArray c;
    c.arrayLength = atoi(getAttrValue("arrayLength", attr));
    c.encodedLength = atoi(getAttrValue("encodedLength", attr));
    c.dataProcessingRef = getAttrValue("dataProcessingRef", attr);
    if (activeEl[activeEl.size() - 3] == mzSpectrum) {
      mzML.run.spectrumList.spectrum.back().binaryDataArrayList.back().binaryDataArray.push_back(c);
    } else if (activeEl[activeEl.size() - 3] == mzChromatogram){
      mzML.run.chromatogramList.back().chromatogram.back().binaryDataArrayList.binaryDataArray.push_back(c);
    } else {
      cout << "Error: unknown parent of binaryDataArray: " << activeEl[activeEl.size() - 3] << endl;
      exit(1);
    }

  } else if (isElement("binaryDataArrayList", el)){
    activeEl.push_back(mzBinaryDataArrayList);
    CnmzBinaryDataArrayList c;
    c.count = atoi(getAttrValue("count", attr));
    if (activeEl[activeEl.size() - 2]==mzSpectrum) {
      mzML.run.spectrumList.spectrum.back().binaryDataArrayList.push_back(c);
    } else if (activeEl[activeEl.size() - 2] == mzChromatogram){
      mzML.run.chromatogramList.back().chromatogram.back().binaryDataArrayList=c;
    } else {
      cout << "Error: unknown parent of binaryDataArrayList: " << activeEl[activeEl.size() - 2] << endl;
      exit(1);
    }

  } else if (isElement("chromatogram", el)) {
    activeEl.push_back(mzChromatogram);
    CnmzChromatogram c;
    c.dataProcessingRef=getAttrValue("dataProcessingRef",attr);
    c.defaultArrayLength = atoi(getAttrValue("defaultArrayLength", attr));
    c.id = getAttrValue("id", attr);
    c.index = atoi(getAttrValue("index", attr));
    mzML.run.chromatogramList.back().chromatogram.push_back(c);

  } else if (isElement("chromatogramList", el)){
    activeEl.push_back(mzChromatogramList);
    bChromatogramList=true;
    CnmzChromatogramList c;
    c.defaultDataProcessingRef = getAttrValue("defaultDataProcessingRef", attr);
    c.count = atoi(getAttrValue("count", attr));
    mzML.run.chromatogramList.push_back(c);
    if (bIndexed) {
      size_t a=0;
      for(a=0;a<indexedmzML.indexList.index.size();a++){
        if(indexedmzML.indexList.index[a].name.compare("chromatogram")==0) break;
      }
      if (a == indexedmzML.indexList.index.size()){
        cout << "Error: chromatogram index is missing or mislabeled." << endl;
        exit(1);
      }
      mzML.run.chromatogramList[0].chromatogramIndex = &indexedmzML.indexList.index[a];
    }
    if (bIterative) {
      bParseAbort = true;
      XML_StopParser(parser, false);
    }
    //if (bIterative) elSkip = mzChromatogramList;

  } else if (isElement("componentList", el)){
    activeEl.push_back(mzComponentList);
    CnmzComponentList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.push_back(c);

  } else if (isElement("cv", el)){
    activeEl.push_back(mzCv);
    CnmzCv c;
    c.fullName=getAttrValue("fullName", attr);
    c.id = getAttrValue("id", attr);
    c.URI = getAttrValue("URI", attr);
    c.version = getAttrValue("version", attr);
    mzML.cvList.cv.push_back(c);

  } else if (isElement("cvList", el)){
    activeEl.push_back(mzCvList);
    CnmzCvList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.cvList=c;

  } else if (isElement("cvParam", el)){
    activeEl.push_back(mzCvParam);
    CnmzCvParam c;
    c.accession = getAttrValue("accession", attr);
    c.cvRef = getAttrValue("cvRef", attr);
    c.name = getAttrValue("name", attr);
    c.unitAccession = getAttrValue("unitAccession", attr);
    c.unitCvRef = getAttrValue("unitCvRef", attr);
    c.unitName = getAttrValue("unitName", attr);
    c.value = getAttrValue("value", attr);
    processCvParam(c);

  } else if (isElement("dataProcessing", el)){
    activeEl.push_back(mzDataProcessing);
    CnmzDataProcessing c;
    c.id = getAttrValue("id", attr);
    mzML.dataProcessingList.dataProcessing.push_back(c);

  } else if (isElement("dataProcessingList", el)){
    activeEl.push_back(mzDataProcessingList);
    CnmzDataProcessingList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.dataProcessingList=c;

  } else if (isElement("detector", el)){
    activeEl.push_back(mzDetector);
    CnmzDetector c;
    c.order = atoi(getAttrValue("order", attr));
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.back().detector.push_back(c);

  } else if (isElement("fileChecksum", el)){
    activeEl.push_back(mzFileChecksum);

  } else if (isElement("fileContent", el)){
    activeEl.push_back(mzFileContent);
    CnmzFileContent c;
    mzML.fileDescription.fileContent=c;

  } else if (isElement("fileDescription", el)){
    activeEl.push_back(mzFileDescription);
    CnmzFileDescription c;
    mzML.fileDescription = c;

  } else if (isElement("index", el)){
    activeEl.push_back(mzIndex);
    CnmzIndex c;
    c.name=getAttrValue("name", attr);
    indexedmzML.indexList.index.push_back(c);
    if (indexedmzML.indexList.index.back().name.compare("spectrum")==0){
      spectrumIndexID = indexedmzML.indexList.index.size()-1;
    } else if (indexedmzML.indexList.index.back().name.compare("chromatogram") == 0){
      chromatogramIndexID = indexedmzML.indexList.index.size() - 1;
    } else {
      cout << "ERROR: Unknown index name: " << c.name << " - Valid names are (case-sensitive) spectrum or chromatogram" << endl;
    }

  } else if (isElement("indexedmzML", el)){
    activeEl.push_back(mzIndexedmzML);
    if(!bIndexed){
      bIndexed=true;
      indexedmzML.xmlns = getAttrValue("xmlns", attr);
      indexedmzML.xmlns_xsi = getAttrValue("xmlns:xsi", attr);
      indexedmzML.xsi_schemaLocation = getAttrValue("xsi:schemaLocation", attr);
      bDoParseIndex=true;
      bParseAbort = true;
      XML_StopParser(parser,false);
    }

  } else if (isElement("indexList", el)){
    activeEl.push_back(mzIndexList);
    CnmzIndexList c;
    c.count = atoi(getAttrValue("count", attr));
    indexedmzML.indexList=c;

  } else if (isElement("indexListOffset", el)){
    activeEl.push_back(mzIndexListOffset);

  } else if (isElement("instrumentConfiguration", el)){
    activeEl.push_back(mzInstrumentConfiguration);
    CnmzInstrumentConfiguration c;
    c.id = getAttrValue("id", attr);
    c.scanSettingsRef = getAttrValue("scanSettingsRef", attr);
    mzML.instrumentConfigurationList.instrumentConfiguration.push_back(c);

  } else if (isElement("instrumentConfigurationList", el)){
    activeEl.push_back(mzInstrumentConfigurationList);
    CnmzInstrumentConfigurationList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.instrumentConfigurationList=c;

  } else if (isElement("isolationWindow", el)){
    activeEl.push_back(mzIsolationWindow);
    CnmzIsolationWindow c;
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.back().isolationWindow.push_back(c);

  } else if (isElement("mzML", el)){
    activeEl.push_back(mzMzML);
    CnmzMzML c;
    c.xmlns = getAttrValue("xmlns", attr);
    c.xmlns_xsi = getAttrValue("xmlns:xsi", attr);
    c.xsi_schemaLocation = getAttrValue("xsi:schemaLocation", attr);
    c.version = getAttrValue("version", attr);
    c.accession = getAttrValue("accession", attr);
    c.id = getAttrValue("id", attr);
    mzML=c;
    if(bIndexed) indexedmzML.mzML=&mzML;
    else {
      if(bIterative) {
        cout << "WARNING: Iterative reading is for indexed mzML files only. Entire mzML will be read to memory." << endl;
        bIterative=false;
      }
    }

  } else if (isElement("offset", el)){
    activeEl.push_back(mzOffset);
    CnmzOffset c;
    c.idRef = getAttrValue("idRef", attr);
    c.scanTime = atof(getAttrValue("scanTime", attr));
    c.spotID = getAttrValue("spotID", attr);
    indexedmzML.indexList.index.back().offset.push_back(c);

  } else if (isElement("precursor", el)){
    activeEl.push_back(mzPrecursor);
    CnmzPrecursor c;
    c.externalSpectrumID = getAttrValue("externalSpectrumID", attr);
    c.sourceFileRef = getAttrValue("sourceFileRef", attr);
    c.spectrumRef = getAttrValue("spectrumRef", attr);
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.push_back(c);

  } else if (isElement("precursorList", el)){
    activeEl.push_back(mzPrecursorList);
    CnmzPrecursorList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.run.spectrumList.spectrum.back().precursorList.push_back(c);

  } else if (isElement("processingMethod", el)){
    activeEl.push_back(mzProcessingMethod);
    CnmzProcessingMethod c;
    c.order = atoi(getAttrValue("order", attr));
    c.softwareRef = getAttrValue("softwareRef", attr);
    mzML.dataProcessingList.dataProcessing.back().processingMethod.push_back(c);

  } else if (isElement("referenceableParamGroup", el)){
    activeEl.push_back(mzReferenceableParamGroup);
    CnmzReferenceableParamGroup c;
    c.id = getAttrValue("id", attr);
    mzML.referencableParamGroupList.back().referenceableParamGroup.push_back(c);

  } else if (isElement("referenceableParamGroupList", el)){
    activeEl.push_back(mzReferenceableParamGroupList);
    CnmzReferenceableParamGroupList c;
    c.count=atoi(getAttrValue("count",attr));
    mzML.referencableParamGroupList.push_back(c);

  } else if (isElement("referenceableParamGroupRef", el)){
    activeEl.push_back(mzReferenceableParamGroupRef);
    CnmzReferenceableParamGroupRef c;
    c.ref = getAttrValue("ref", attr);
    switch (activeEl[activeEl.size() - 2]) {
    case mzInstrumentConfiguration:
      mzML.instrumentConfigurationList.instrumentConfiguration.back().referenceableParamGroupRef.push_back(c);
      break;
    default:
      cerr << "Error: parent of referenceableParamGroupRef not processed: " << elements[activeEl[activeEl.size() - 2]] << endl;
      break;
    }

  } else if (isElement("run", el)){
    activeEl.push_back(mzRun);
    CnmzRun c;
    c.defaultInstrumentConfigurationRef = getAttrValue("defaultInstrumentConfigurationRef", attr);
    c.defaultSourceFileRef = getAttrValue("defaultSourceFileRef", attr);
    c.id = getAttrValue("id", attr);
    c.sampleRef = getAttrValue("sampleRef", attr);
    c.startTimeStamp.parseDateTime(getAttrValue("startTimeStamp", attr));
    mzML.run=c;
    //if(bIterative) XML_StopParser(parser,false);

  } else if (isElement("sample", el)){
    activeEl.push_back(mzSample);
    CnmzSample c;
    c.id = getAttrValue("id", attr);
    c.name = getAttrValue("name", attr);
    mzML.sampleList.back().sample.push_back(c);

  } else if (isElement("sampleList", el)){
    activeEl.push_back(mzSampleList);
    CnmzSampleList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.sampleList.push_back(c);

  } else if (isElement("scan", el)){
    activeEl.push_back(mzScan);
    CnmzScan c;
    c.externalSpectrumID = getAttrValue("externalSpectrumID", attr);
    c.instrumentConfigurationRef = getAttrValue("instrumentConfigurationRef", attr);
    c.sourceFileRef = getAttrValue("sourceFileRef", attr);
    c.spectrumRef = getAttrValue("spectrumRef", attr);
    mzML.run.spectrumList.spectrum.back().scanList.back().scan.push_back(c);
  
  } else if (isElement("scanList", el)){
    activeEl.push_back(mzScanList);
    CnmzScanList c;
    c.count = atoi(getAttrValue("count", attr));
    if(c.count==0){
      std::cerr << "WARNING: spectrum::scanList for spectrum::id=" << mzML.run.spectrumList.spectrum.back().id << " must have at least one scan. Removing this scanList." << std::endl;
    } else mzML.run.spectrumList.spectrum.back().scanList.push_back(c);

  } else if (isElement("scanWindow", el)){
    activeEl.push_back(mzScanWindow);
    CnmzScanWindow c;
    mzML.run.spectrumList.spectrum.back().scanList.back().scan.back().scanWindowList.back().scanWindow.push_back(c);

  } else if (isElement("scanWindowList", el)){
    activeEl.push_back(mzScanWindowList);
    CnmzScanWindowList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.run.spectrumList.spectrum.back().scanList.back().scan.back().scanWindowList.push_back(c);
  
  } else if (isElement("selectedIon", el)){
    activeEl.push_back(mzSelectedIon);
    CnmzSelectedIon c;
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.back().selectedIonList.back().selectedIon.push_back(c);

  } else if (isElement("selectedIonList", el)){
    activeEl.push_back(mzSelectedIonList);
    CnmzSelectedIonList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.run.spectrumList.spectrum.back().precursorList.back().precursor.back().selectedIonList.push_back(c);

  } else if (isElement("source", el)){
    activeEl.push_back(mzSource);
    CnmzSource c;
    c.order = atoi(getAttrValue("order", attr));
    mzML.instrumentConfigurationList.instrumentConfiguration.back().componentList.back().source.push_back(c);

  } else if (isElement("sourceFile", el)){
    activeEl.push_back(mzSourceFile);
    CnmzSourceFile c;
    c.id = getAttrValue("id", attr);
    c.location = getAttrValue("location", attr);
    c.name = getAttrValue("name", attr);
    mzML.fileDescription.sourceFileList.back().sourceFile.push_back(c);

  } else if (isElement("software", el)){
    activeEl.push_back(mzSoftware);
    CnmzSoftware c;
    c.id = getAttrValue("id", attr);
    c.version = getAttrValue("version", attr);
    if(c.version.empty()){
      std::cerr << "WARNING: software::version was empty when reading mzML. Setting value to 0." << std::endl;
      c.version="0";
    }
    mzML.softwareList.software.push_back(c);

  } else if (isElement("softwareList", el)){
    activeEl.push_back(mzSoftwareList);
    CnmzSoftwareList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.softwareList=c;

  } else if (isElement("softwareRef", el)){
    activeEl.push_back(mzSoftwareRef);
    CnmzSoftwareRef c;
    c.ref = getAttrValue("ref", attr);
    mzML.instrumentConfigurationList.instrumentConfiguration.back().softwareRef.push_back(c);

  } else if (isElement("sourceFileList", el)){
    activeEl.push_back(mzSourceFileList);
    CnmzSourceFileList c;
    c.count = atoi(getAttrValue("count", attr));
    mzML.fileDescription.sourceFileList.push_back(c);

  } else if (isElement("spectrum", el)){
    activeEl.push_back(mzSpectrum);
    CnmzSpectrum c;
    c.dataProcessingRef = getAttrValue("dataProcessingRef", attr);
    c.defaultArrayLength = atoi(getAttrValue("defaultArrayLength", attr));
    c.id = getAttrValue("id", attr);
    c.index = atoi(getAttrValue("index", attr));
    c.sourceFileRef = getAttrValue("sourceFileRef", attr);
    c.spotID = getAttrValue("spotID", attr);
    mzML.run.spectrumList.spectrum.push_back(c);

  } else if (isElement("spectrumList", el)){
    activeEl.push_back(mzSpectrumList);
    bSpectrumList=true;
    CnmzSpectrumList c;
    c.count=atoi(getAttrValue("count",attr));
    c.defaultDataProcessingRef = getAttrValue("defaultDataProcessingRef", attr);
    mzML.run.spectrumList = c;
    if (bIndexed) {
      size_t a = 0;
      for (a = 0; a<indexedmzML.indexList.index.size(); a++){
        if (indexedmzML.indexList.index[a].name.compare("spectrum") == 0) break;
      }
      if (a == indexedmzML.indexList.index.size()){
        cout << "Error: spectrum index is missing or mislabeled." << endl;
        exit(1);
      }
      mzML.run.spectrumList.spectrumIndex = &indexedmzML.indexList.index[a];
    }
    if (bIterative) {
      bParseAbort = true;
      XML_StopParser(parser, false);
    }
    //if(bIterative) elSkip=mzSpectrumList;

  } else if (isElement("userParam", el)){
    activeEl.push_back(mzUserParam);
    CnmzUserParam c;
    c.type = getAttrValue("type", attr);
    c.name = getAttrValue("name", attr);
    c.unitAccession = getAttrValue("unitAccession", attr);
    c.unitCvRef = getAttrValue("unitCvRef", attr);
    c.unitName = getAttrValue("unitName", attr);
    c.value = getAttrValue("value", attr);
    processUserParam(c);

  } else {
    cout << "WARNING: Element undefined: " << el << endl;
    //exit(1);
  }
}

bool NeoMzMLParser::read(const char* fn, bool iterative){
  
  // clear data
  if(xml_fptr!=NULL) fclose(xml_fptr);
  xml_fptr = fopen(fn, "rt");
  if (xml_fptr == NULL){
    cerr << "Error parse(): No open file." << endl;
    return false;
  }
  fileName=fn;

  scanIndex=-1;
  chromatIndex=-1;
  bDoParseIndex=false;
  bIterative=iterative;
  bParseIndexList = false;
  elSkip = MZML_NUM_ELEMENTS;
  chromatogramIndexID=SIZE_MAX;
  bSpectrumList=false;
  bChromatogramList=false;

  parse(0);
  if(bDoParseIndex) {
    parseIndex();
    parse(0);

    if(!bChromatogramList) parseChromatogramList();
  }

  return true;
}

bool NeoMzMLParser::writeChromatogram(CnmzChromatogram* chromat){
  if (WState!=mzWS_Chromat){
    cout << "ERROR: Attempting iterative writing of chromatogram in the wrong state." << endl;
    return false;
  }

  if(!bWChromatList){
    if (bWTabs)mzML.run.chromatogramList[0].write(Wptr, 3, true);
    else mzML.run.chromatogramList[0].write(Wptr, -1, true);
    bWChromatList=true;
  }

  int t = -1;
  if (bWTabs) t = 4;
  size_t a;
  for (a = 0; a<indexedmzML.indexList.count; a++){
    if (indexedmzML.indexList.index[a].name.compare("chromatogram") == 0) break;
  }

  //make index if needed
  if (WindexList->count==1){
    CnmzIndex c;
    c.name = "chromatogram";
    WindexList->index.push_back(c);
    //mzML.run.chromatogramList[0].chromatogramIndex=&indexedmzML.indexList.index.back();
    WindexList->count++;
  }

  //addOffset
  int tIndex=chromat->index;
  chromat->index=WChromatCount;
  CnmzOffset o;
  o.idRef = chromat->id;
  o.content = to_string(nmzftell(Wptr));
  WindexList->index[1].offset.push_back(o);
  chromat->write(Wptr, t);
  WChromatCount++;
  chromat->index=tIndex;
  return true;
}

mzMLWriteState NeoMzMLParser::writeNextState(){
  int t=-1;
  switch (WState){
  case mzWS_Spectrum:
    if (bWTabs) NMZprintTabs(Wptr, 3);
    fprintf(Wptr, "</spectrumList>\n");
    WState=mzWS_Chromat;
    break;
  case mzWS_Chromat:
    if(bWChromatList) {
      if (bWTabs) NMZprintTabs(Wptr, 3);
      fprintf(Wptr, "</chromatogramList>\n");
    }
    WState=mzWS_Run;
    break;
  case mzWS_Run:
    if (bWTabs) NMZprintTabs(Wptr, 2);
    fprintf(Wptr, "</run>\n");
    if (bWTabs) NMZprintTabs(Wptr, 1);
    fprintf(Wptr, "</mzML>\n");
    if(bWTabs) t=1;
    WindexList->write(Wptr,t,true);
    WindexListOffset.content = to_string(WindexList->getIndexListOffset());
    WindexListOffset.write(Wptr,t,true);
    WfileChecksum.content.clear();
    WfileChecksum.write(Wptr,t,true);
    //indexedmzML.indexList.write(Wptr,t);
    //indexedmzML.indexListOffset.content=to_string(indexedmzML.indexList.getIndexListOffset());
    //indexedmzML.indexListOffset.write(Wptr, t);
    //indexedmzML.fileChecksum.content="nochecksum";
    //indexedmzML.fileChecksum.write(Wptr,t);
    fprintf(Wptr,"</indexedmzML>\n");
    //WspectrumList.count=WSpecCount;
    //WspectrumList.writeUpdate(Wptr);
    //mzML.run.spectrumList.count=WSpecCount;
    mzML.run.spectrumList.writeUpdate(Wptr,WSpecCount);
    //if (bWChromatList){
    //  WchromatogramList.count=WChromatCount;
    //  WchromatogramList.writeUpdate(Wptr);
    //}
    if(!mzML.run.chromatogramList.empty()) {
      //mzML.run.chromatogramList[0].count=WChromatCount;
      mzML.run.chromatogramList[0].writeUpdate(Wptr,WChromatCount);
    }
    fclose(Wptr);
    Wptr=NULL;
    WState=mzWS_Done;
    //Clear all temporary lists
    delete WindexList;
    WindexList=NULL;
    break;
  default:
    break;
  }
  return WState;
}

bool NeoMzMLParser::writeSpectrum(CnmzSpectrum* spec){
  if (WState != mzWS_Spectrum){
    cout << "ERROR: Attempting iterative writing of chromatogram in the wrong state." << endl;
    return false;
  }
  int t=-1;
  if (bWTabs) t=4;
  
  //make index if needed
  if(WindexList->count==0){
    CnmzIndex c;
    c.name = "spectrum";
    WindexList->index.push_back(c);
    //mzML.run.spectrumList.spectrumIndex = &indexedmzML.indexList.index.back();
    WindexList->count++;
  }

  //addOffset - also temporarily set the index to the next one
  int tIndex=spec->index;
  spec->index = WSpecCount;
  CnmzOffset o;
  o.idRef=spec->id;
  o.content = to_string(nmzftell(Wptr));
  WindexList->index[0].offset.push_back(o);
  spec->write(Wptr,t);
  WSpecCount++;
  spec->index=tIndex;
  return true;
}

bool NeoMzMLParser::write(const char* fn, bool tabs, bool iterative){
  if(Wptr!=NULL) {
    cout << "A file is currently being exported. Finish the export before starting the next one." << endl;
    return false;
  }
  Wptr = fopen(fn, "wt");
  if (Wptr == NULL) return false;

  bWTabs=tabs;
  bWIterative=iterative;
  bWChromatList=false;

  if(!bIndexed) {
    createIndex();
  }

  fprintf(Wptr, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
  if (tabs) indexedmzML.write(Wptr, 0, iterative);
  else indexedmzML.write(Wptr, -1, iterative);

  if(iterative) {
    WState=mzWS_Spectrum;
    WindexList = new CnmzIndexList();
    //indexedmzML.indexList.index.clear();
    //indexedmzML.indexList.count=0;
    WSpecCount=0;
    WChromatCount=0;
  } else {
    fclose(Wptr);
    Wptr=NULL;
  }

  return true;

}

void NeoMzMLParser::xmlError(XML_Error error){
  cerr << fileName << "(" << XML_GetCurrentLineNumber(parser) << ") : error " << (int)error << ": ";
  switch (error) {
  case XML_ERROR_SYNTAX:
    cerr << "Syntax error parsing XML.";
    break;
  case XML_ERROR_INVALID_TOKEN:
    cerr << "XML invalid token.";
    break;
  case XML_ERROR_UNCLOSED_TOKEN:
    cerr << "XML unclosed token.";
    break;
  case XML_ERROR_TAG_MISMATCH:
    cerr << "XML tag mismatch.";
    break;
  case XML_ERROR_DUPLICATE_ATTRIBUTE:
    cerr << "XML duplicate attribute.";
    break;
  case XML_ERROR_JUNK_AFTER_DOC_ELEMENT:
    cerr << "XML junk after doc element.";
    break;
  case XML_ERROR_BAD_CHAR_REF:
    cerr << "XML bad character reference.";
    break;
  case XML_ERROR_NOT_SUSPENDED:
    cerr << "XML not suspended.";
    break;
  case XML_ERROR_FINISHED:
    cerr << "XML finished.";
    break;
  default:
    cerr << "XML Parsing error.";
    break;
  }
  cerr << "\n";
}