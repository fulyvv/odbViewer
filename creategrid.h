#ifndef CREATEGRID_H
#define CREATEGRID_H

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkXMLUnstructuredGridWriter.h>
#include <vtkIntArray.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkCellData.h>

#include "odbmanager.h"

class CreateVTKUnstucturedGrid {
public:
    explicit CreateVTKUnstucturedGrid(const readOdb& odb);

    void addPointScalar(const std::string& name, const std::vector<double>& values);

    void addCellScalar(const std::string& name, const std::vector<double>& values);
    void addCellScalar(const std::string& name, const std::vector<int>& values);

    bool writeToFile(const std::string& filename) const;

    bool addFieldData(const FieldData& fieldData);

    bool addDisplacementField(const FieldData& fieldData, double scaleFactor = 1.0);

    bool addStressField(const FieldData& fieldData, const std::string& component = "ALL");

    void calculateVonMisesStress(const FieldData& stressField);

    vtkUnstructuredGrid* getGrid() const { return m_grid.Get(); }
private:
    static int abaqusToVTKCellType(const std::string& abaqusType);

    void buildGeometry();
    void applyDisplacement(const std::vector<std::vector<double>>& displacements, double scaleFactor);

    const readOdb& m_odb;
    vtkSmartPointer<vtkUnstructuredGrid> m_grid;
};

#endif // CREATEGRID_H
