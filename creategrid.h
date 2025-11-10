#ifndef CREATEGRID_H
#define CREATEGRID_H

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkXMLUnstructuredGridWriter.h>
#include <vtkFloatArray.h>
#include <vtkUnsignedCharArray.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkIdTypeArray.h>
#include <vtkCellType.h>
#include <unordered_map>

#include "odbmanager.h"

class CreateVTKUnstucturedGrid {
public:
    explicit CreateVTKUnstucturedGrid(const readOdb& odb);
    void addCellScalar(const std::string& name, const std::vector<float>& values);

    bool writeToFile(const std::string& filename) const;

    bool addFieldData(const FieldData& fieldData);
    bool addDisplacementField(const FieldData& fieldData, double scaleFactor = 1.0);
    bool addStressField(const FieldData& fieldData, const std::string& component = "ALL");
    void calculateVonMisesStress(const FieldData& stressField);

    vtkUnstructuredGrid* getGrid() const { return m_grid.Get(); }
private:
    const readOdb& m_odb;
    vtkSmartPointer<vtkUnstructuredGrid> m_grid;

    void buildGeometry();
    static int abaqusToVTKCellType(const std::string& abaqusType);
    void applyDisplacement(const FieldData& displacementField, double scaleFactor);
    vtkSmartPointer<vtkFloatArray> makeFloatArray(const std::string& name, int numComponents,
        std::size_t tupleCount, const std::vector<float>& values, const std::vector<uint8_t>& validFlags);
};
#endif // CREATEGRID_H
