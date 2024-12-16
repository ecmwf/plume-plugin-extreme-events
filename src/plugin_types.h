#pragma once


#ifdef WITH_EE_PLUGIN_SINGLE_PRECISION
#define FIELD_TYPE_REAL float
#else
#define FIELD_TYPE_REAL double
#endif