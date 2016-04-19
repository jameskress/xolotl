#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Regression

#include <boost/test/included/unit_test.hpp>
#include <W110TrapMutationHandler.h>
#include <HDF5NetworkLoader.h>
#include <XolotlConfig.h>
#include <DummyHandlerRegistry.h>
#include <mpi.h>

using namespace std;
using namespace xolotlCore;

/**
 * This suite is responsible for testing the W110TrapMutationHandler.
 */
BOOST_AUTO_TEST_SUITE(W110TrapMutationHandler_testSuite)

/**
 * Method checking the initialization and the compute modified trap-mutation methods.
 */
BOOST_AUTO_TEST_CASE(checkModifiedTrapMutation) {
	// Initialize MPI for HDF5
	int argc = 0;
	char **argv;
	MPI_Init(&argc, &argv);

	// Create the network loader
	HDF5NetworkLoader loader =
			HDF5NetworkLoader(make_shared<xolotlPerf::DummyHandlerRegistry>());
	// Define the filename to load the network from
	string sourceDir(XolotlSourceDirectory);
	string pathToFile("/tests/testfiles/tungsten.h5");
	string filename = sourceDir + pathToFile;
	// Give the filename to the network loader
	loader.setFilename(filename);

	// Load the network
	auto network = (PSIClusterReactionNetwork *) loader.load().get();
	// Get all the reactants
	auto allReactants = network->getAll();
	// Get its size
	const int size = network->getAll()->size();
	// Initialize the rate constants
	for (int i = 0; i < size; i++) {
		// This part will set the temperature in each reactant
		// and recompute the diffusion coefficient
		allReactants->at(i)->setTemperature(1000.0);
	}
	for (int i = 0; i < size; i++) {
		// Now that the diffusion coefficients of all the reactants
		// are updated, the reaction and dissociation rates can be
		// recomputed
		auto cluster = (xolotlCore::PSICluster *) allReactants->at(i);
		cluster->computeRateConstants();
	}

	// Suppose we have a grid with 13 grip points and distance of
	// 0.1 nm between grid points
	std::vector<double> grid;
	for (int l = 0; l < 13; l++) {
		grid.push_back((double) l * 0.1);
	}
	// Set the surface position
	int surfacePos = 0;

	// Create the modified trap-mutation handler
	W110TrapMutationHandler trapMutationHandler;

	// Initialize it
	trapMutationHandler.initialize(surfacePos, network, grid);

	// The arrays of concentration
	double concentration[13*size];
	double newConcentration[13*size];

	// Initialize their values
	for (int i = 0; i < 13*size; i++) {
		concentration[i] = (double) i * i;
		newConcentration[i] = 0.0;
	}

	// Get pointers
	double *conc = &concentration[0];
	double *updatedConc = &newConcentration[0];

	// Get the offset for the eighth grid point
	double *concOffset = conc + 7 * size;
	double *updatedConcOffset = updatedConc + 7 * size;

	// Putting the concentrations in the network so that the rate for
	// desorption is computed correctly
	network->updateConcentrationsFromArray(concOffset);

	// Compute the modified trap mutation at the eighth grid point
	trapMutationHandler.computeTrapMutation(network, 7,
			concOffset, updatedConcOffset);

	// Check the new values of updatedConcOffset
	BOOST_REQUIRE_CLOSE(updatedConcOffset[0], 6.26006e+30, 0.01); // Create I
	BOOST_REQUIRE_CLOSE(updatedConcOffset[7], -6.26006e+30, 0.01); // He2
	BOOST_REQUIRE_CLOSE(updatedConcOffset[16], 6.26006e+30, 0.01); // Create He2V

	// Get the offset for the tenth grid point
	concOffset = conc + 9 * size;
	updatedConcOffset = updatedConc + 9 * size;

	// Putting the concentrations in the network so that the rate for
	// desorption is computed correctly
	network->updateConcentrationsFromArray(concOffset);

	// Compute the modified trap mutation at the tenth grid point
	trapMutationHandler.computeTrapMutation(network, 9,
			concOffset, updatedConcOffset);

	// Check the new values of updatedConcOffset
	BOOST_REQUIRE_CLOSE(updatedConcOffset[0], 1.00537e+23, 0.01); // Create I
	BOOST_REQUIRE_CLOSE(updatedConcOffset[7], 0.0, 0.01); // He2
	BOOST_REQUIRE_CLOSE(updatedConcOffset[16], 0.0, 0.01); // Doesn't create He2V
	BOOST_REQUIRE_CLOSE(updatedConcOffset[10], -3.35159e+22, 0.01); // He5
	BOOST_REQUIRE_CLOSE(updatedConcOffset[19], 3.35159e+22, 0.01); // Create He5V

	// Initialize the indices and values to set in the Jacobian
	int nHelium = network->getAll(heType).size();
	int indices[3*nHelium];
	double val[3*nHelium];
	// Get the pointer on them for the compute modified trap-mutation method
	int *indicesPointer = &indices[0];
	double *valPointer = &val[0];

	// Compute the partial derivatives for the modified trap-mutation at the grid point 9
	int nMutating = trapMutationHandler.computePartialsForTrapMutation(network, valPointer,
			indicesPointer, 9);

	// Check the values for the indices
	BOOST_REQUIRE_EQUAL(indices[0], 8); // He3
	BOOST_REQUIRE_EQUAL(indices[1], 17); // He3V
	BOOST_REQUIRE_EQUAL(indices[2], 0); // I
	BOOST_REQUIRE_EQUAL(indices[3], 9); // He4
	BOOST_REQUIRE_EQUAL(indices[4], 18); // He4V
	BOOST_REQUIRE_EQUAL(indices[5], 0); // I

	// Check values
	BOOST_REQUIRE_CLOSE(val[0], -9.67426e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[1], 9.67426e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[2], 9.67426e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[3], -9.67426e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[4], 9.67426e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[5], 9.67426e+13, 0.01);

	// Change the temperature of the network
	network->setTemperature(500.0);

	// Update the bursting rate
	trapMutationHandler.updateTrapMutationRate(network);

	// Compute the partial derivatives for the bursting a the grid point 9
	nMutating = trapMutationHandler.computePartialsForTrapMutation(network, valPointer,
			indicesPointer, 9);

	// Check values
	BOOST_REQUIRE_CLOSE(val[0], -2.14016e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[1], 2.14016e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[2], 2.14016e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[3], -2.14016e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[4], 2.14016e+13, 0.01);
	BOOST_REQUIRE_CLOSE(val[5], 2.14016e+13, 0.01);

	// Finalize MPI
	MPI_Finalize();

	return;
}

BOOST_AUTO_TEST_SUITE_END()