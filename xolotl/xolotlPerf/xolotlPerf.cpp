#include <iostream>
#include <sstream>
#include "xolotlPerf/perfConfig.h"
#include "xolotlPerf/xolotlPerf.h"
#include "xolotlPerf/dummy/DummyHandlerRegistry.h"
#include "xolotlPerf/os/OSHandlerRegistry.h"

#if defined(HAVE_PAPI)
#include "xolotlPerf/papi/PAPIHandlerRegistry.h"
#endif // defined(HAVE_PAPI)

namespace xolotlPerf {

static std::shared_ptr<IHandlerRegistry> theHandlerRegistry;

// Create the desired type of handler registry.
void initialize(IHandlerRegistry::RegistryType rtype) {
	switch (rtype) {
	case IHandlerRegistry::dummy:
		theHandlerRegistry = std::make_shared<DummyHandlerRegistry>();
		break;

	case IHandlerRegistry::std:
		theHandlerRegistry = std::make_shared<OSHandlerRegistry>();
		break;

	case IHandlerRegistry::os:
		theHandlerRegistry = std::make_shared<OSHandlerRegistry>();
		break;

	case IHandlerRegistry::papi:
#if defined(HAVE_PAPI)
		theHandlerRegistry = std::make_shared<PAPIHandlerRegistry>();
#else
		throw std::invalid_argument(
				"PAPI handler registry requested but no PAPI support was found when the program was built.");
#endif // defined(HAVE_PAPI)
		break;

	default:
		throw std::invalid_argument(
				"unrecognized performance handler registry type requested");
		break;
	}
}

// Provide access to our handler registry.
std::shared_ptr<IHandlerRegistry> getHandlerRegistry(void) {
	if (!theHandlerRegistry) {
		throw std::runtime_error(
				"Request for xolotlPerf handler registry before xolotlPerf library has been initialized");
	}
	return theHandlerRegistry;
}

// OpenMPI 2.x and 3.0 define its MPI_Datatype constants using a
// C-style cast to void*.  Clang++ objects to using these with
// in-class initializers and constexpr in our classes like ITimer.
// So we have to define them elsewhere (i.e., here).
const MPI_Datatype ITimer::MPIValType = MPI_DOUBLE;
const MPI_Datatype IEventCounter::MPIValType = MPI_UNSIGNED_LONG;
const MPI_Datatype IHardwareCounter::MPIValType = MPI_LONG_LONG_INT;

} // end namespace xolotlPerf

