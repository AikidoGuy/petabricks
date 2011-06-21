#include "regiondataraw.h"

#include "matrixio.h"

using namespace petabricks;

RegionDataRaw::RegionDataRaw(int dimensions, IndexT* size) {
  init(dimensions, size, NULL, NULL);
}

RegionDataRaw::RegionDataRaw(int dimensions, IndexT* size, ElementT* data) {
  init(dimensions, size, data, NULL);
}

RegionDataRaw::RegionDataRaw(int dimensions, IndexT* size, IndexT* partOffset) {
  init(dimensions, size, NULL, partOffset);
}

RegionDataRaw::RegionDataRaw(const char* filename) {
  MatrixIO* matrixio = new MatrixIO(filename, "r");
  MatrixReaderScratch o = matrixio->readToMatrixReaderScratch();
  init(o.dimensions, o.sizes, o.storage->data(), NULL);
}

RegionDataRaw::~RegionDataRaw() {
  delete [] _multipliers;
  if (_isPart) delete [] _partOffset;
}

void RegionDataRaw::init(int dimensions, IndexT* size, ElementT* data, IndexT* partOffset) {
  _D = dimensions;
  _type = RegionDataTypes::REGIONDATARAW;

  _size = new IndexT[_D];
  memcpy(_size, size, sizeof(IndexT) * _D);

  if (data) {
    int numData = allocData();
    memcpy(_storage->data(), data, sizeof(ElementT) * numData);
  }

  _multipliers = new IndexT[_D];
  _multipliers[0] = 1;
  for (int i = 1; i < _D; i++) {
    _multipliers[i] = _multipliers[i - 1] * _size[i - 1];
  }

  if (partOffset) {
    _isPart = true;
    _partOffset = new IndexT[_D];
    memcpy(_partOffset, partOffset, sizeof(IndexT) * _D);
  } else {
    _isPart = false;
  }
}

int RegionDataRaw::allocData() {
  int numData = 1;
  for (int i = 0; i < _D; i++) {
    numData *= _size[i];
  }

  _storage = new MatrixStorage(numData);
  return numData;
}

ElementT* RegionDataRaw::coordToPtr(const IndexT* coord){
  IndexT offset = 0;

  if (_isPart) {
    // this is a part of a region
    // convert original coord to this part coord before calculating offset

    for(int i = 0; i < _D; i++){
      offset += _multipliers[i] * (coord[i] - _partOffset[i]);
    }
  } else {
    for(int i = 0; i < _D; i++){
      offset += _multipliers[i] * coord[i];
    }
  }

  return _storage->data() + offset;
}

ElementT RegionDataRaw::readCell(const IndexT* coord) {
  return *this->coordToPtr(coord);
}

void RegionDataRaw::writeCell(const IndexT* coord, ElementT value) {
  ElementT* cell = this->coordToPtr(coord);
  *cell = value;
}

DataHostList RegionDataRaw::hosts(IndexT* /*begin*/, IndexT* /*end*/) {
  DataHostListItem item = {HostPid::self(), 1};
  return DataHostList(1, item);
}

