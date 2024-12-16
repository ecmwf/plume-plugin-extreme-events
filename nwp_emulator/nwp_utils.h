/*
 * (C) Copyright 2023- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include "atlas/field/Field.h"
#include "atlas/functionspace/StructuredColumns.h"

namespace nwp_utils {

// A helper class for creating Atlas Fields
// used by the NWP emulator 
class FieldGenerator {

public: 

    FieldGenerator() = default;
    ~FieldGenerator() = default;

    void setupFunctionSpace();

    atlas::Field createField2D(const std::string& name);

    atlas::Field createField3D(const std::string& name);

    int levels() const {return nLevels_;}

private:

    atlas::Field createField(const std::string& name, int n_levels);

private:

    int gridSizeX_{32};  // grid size X
    int gridSizeY_{64};  // grid size Y
    int nHalo_{3};       // N halo points
    int nLevels_{19};    // N vertical levels for 3D fields

    atlas::functionspace::StructuredColumns fs_;

};

void updateField(atlas::Field& field, int tstep);

}