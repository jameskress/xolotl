// Includes
#include "SuperCluster.h"
#include "PSIClusterReactionNetwork.h"
#include <iostream>
#include <Constants.h>
#include <MathUtils.h>

using namespace xolotlCore;

/**
 * The helium momentum partials. It is computed only for super clusters.
 */
std::vector<double> heMomentumPartials;

/**
 * The vacancy momentum partials. It is computed only for super clusters.
 */
std::vector<double> vMomentumPartials;

SuperCluster::SuperCluster(double numHe, double numV, int nTot, int heWidth,
		int vWidth, double radius,
		std::shared_ptr<xolotlPerf::IHandlerRegistry> registry) :
		PSICluster(registry), numHe(numHe), numV(numV), nTot(nTot), l0(0.0), l1He(
				0.0), l1V(0.0), dispersionHe(0.0), dispersionV(0.0) {
	// Set the cluster size as the sum of
	// the number of Helium and Vacancies
	size = (int) (numHe + numV);

	// Update the composition map
	compositionMap[heType] = (int) (numHe * (double) nTot);
	compositionMap[vType] = (int) (numV * (double) nTot);

	// Set the width
	sectionHeWidth = heWidth;
	sectionVWidth = vWidth;

	// Set the reaction radius and formation energy
	reactionRadius = radius;
	formationEnergy = 0.0;

	// Set the diffusion factor and the migration energy
	migrationEnergy = std::numeric_limits<double>::infinity();
	diffusionFactor = 0.0;

	// Set the reactant name appropriately
	std::stringstream nameStream;
	nameStream << "He_" << numHe << "V_" << numV;
	name = nameStream.str();
	// Set the typename appropriately
	typeName = "Super";

	return;
}

SuperCluster::SuperCluster(SuperCluster &other) :
		PSICluster(other) {
	numHe = other.numHe;
	numV = other.numV;
	nTot = other.nTot;
	sectionHeWidth = other.sectionHeWidth;
	sectionVWidth = other.sectionVWidth;
	l0 = other.l0;
	l1He = other.l1He;
	l1V = other.l1V;
	dispersionHe = other.dispersionHe;
	dispersionV = other.dispersionV;
	reactingMap = other.reactingMap;
	combiningMap = other.combiningMap;
	dissociatingMap = other.dissociatingMap;
	emissionMap = other.emissionMap;
	effReactingMap = other.effReactingMap;
	effCombiningMap = other.effCombiningMap;
	effDissociatingMap = other.effDissociatingMap;
	effEmissionMap = other.effEmissionMap;
	effReactingList = other.effReactingList;
	effCombiningList = other.effCombiningList;
	effDissociatingList = other.effDissociatingList;
	effEmissionList = other.effEmissionList;

	return;
}

std::shared_ptr<IReactant> SuperCluster::clone() {
	std::shared_ptr<IReactant> reactant(new SuperCluster(*this));

	return reactant;
}

double SuperCluster::getConcentration(double distHe, double distV) const {
	return l0 + (distHe * l1He) + (distV * l1V);
}

double SuperCluster::getHeMomentum() const {
	return l1He;
}

double SuperCluster::getVMomentum() const {
	return l1V;
}

double SuperCluster::getTotalConcentration() const {
	// Initial declarations
	int heIndex = 0, vIndex = 0;
	double heDistance = 0.0, vDistance = 0.0, conc = 0.0;

	// Loop on the vacancy width
	for (int k = 0; k < sectionVWidth; k++) {
		// Compute the vacancy index
		vIndex = (int) (numV - (double) sectionVWidth / 2.0) + k + 1;

		// Loop on the helium width
		for (int j = 0; j < sectionHeWidth; j++) {
			// Compute the helium index
			heIndex = (int) (numHe - (double) sectionHeWidth / 2.0) + j + 1;

			// Check if this cluster exists
			if (effReactingMap.find(std::make_pair(heIndex, vIndex))
					== effReactingMap.end())
				continue;

			// Compute the distances
			heDistance = getHeDistance(heIndex);
			vDistance = getVDistance(vIndex);

			// Add the concentration of each cluster in the group times its number of helium
			conc += getConcentration(heDistance, vDistance);
		}
	}

	return conc;
}

double SuperCluster::getTotalHeliumConcentration() const {
	// Initial declarations
	int heIndex = 0, vIndex = 0;
	double heDistance = 0.0, vDistance = 0.0, conc = 0.0;

	// Loop on the vacancy width
	for (int k = 0; k < sectionVWidth; k++) {
		// Compute the vacancy index
		vIndex = (int) (numV - (double) sectionVWidth / 2.0) + k + 1;

		// Loop on the helium width
		for (int j = 0; j < sectionHeWidth; j++) {
			// Compute the helium index
			heIndex = (int) (numHe - (double) sectionHeWidth / 2.0) + j + 1;

			// Check if this cluster exists
			if (effReactingMap.find(std::make_pair(heIndex, vIndex))
					== effReactingMap.end())
				continue;

			// Compute the distances
			heDistance = getHeDistance(heIndex);
			vDistance = getVDistance(vIndex);

			// Add the concentration of each cluster in the group times its number of helium
			conc += getConcentration(heDistance, vDistance) * (double) heIndex;
		}
	}

	return conc;
}

double SuperCluster::getHeDistance(int he) const {
	if (sectionHeWidth == 1)
		return 0.0;
	return 2.0 * (double) (he - numHe) / ((double) sectionHeWidth - 1.0);
}

double SuperCluster::getVDistance(int v) const {
	if (sectionVWidth == 1)
		return 0.0;
	return 2.0 * (double) (v - numV) / ((double) sectionVWidth - 1.0);
}

void SuperCluster::createReactionConnectivity() {
	// Aggregate the reacting pairs and combining reactants from the heVVector
	// Loop on the heVVector
	for (int i = 0; i < heVVector.size(); i++) {
		// Get the cluster composition
		auto comp = heVVector[i]->getComposition();
		// Get both production vectors
		auto react = heVVector[i]->reactingPairs;
		auto combi = heVVector[i]->combiningReactants;

		// Create the key to the map
		auto key = std::make_pair(comp[heType], comp[vType]);

		// Set them in the super cluster map
		reactingMap[key] = react;
		combiningMap[key] = combi;
	}

	return;
}

void SuperCluster::createDissociationConnectivity() {
	// Aggregate the dissociating and emission pairs from the heVVector
	// Loop on the heVVector
	for (int i = 0; i < heVVector.size(); i++) {
		// Get the cluster composition
		auto comp = heVVector[i]->getComposition();
		// Get both dissociation vectors
		auto disso = heVVector[i]->dissociatingPairs;
		auto emi = heVVector[i]->emissionPairs;

		// Create the key to the map
		auto key = std::make_pair(comp[heType], comp[vType]);

		// Set them in the super cluster map
		dissociatingMap[key] = disso;
		emissionMap[key] = emi;
	}

	return;
}

void SuperCluster::computeRateConstants() {
	// Local declarations
	PSICluster *firstReactant, *secondReactant, *combiningReactant,
			*dissociatingCluster, *otherEmittedCluster, *firstCluster,
			*secondCluster;
	double rate = 0.0;
	int heIndex = 0, vIndex = 0;
	// Initialize the value for the biggest production rate
	double biggestProductionRate = 0.0;
	// Initialize the dispersion sum
	int nHeSquare = 0, nVSquare = 0;

	// Loop on the vacancy width
	for (int k = 0; k < sectionVWidth; k++) {
		// Compute the vacancy index
		vIndex = (int) (numV - (double) sectionVWidth / 2.0) + k + 1;

		// Loop on the helium width
		for (int j = 0; j < sectionHeWidth; j++) {
			// Compute the helium index
			heIndex = (int) (numHe - (double) sectionHeWidth / 2.0) + j + 1;

			// Create the key for the maps
			auto key = std::make_pair(heIndex, vIndex);

			// Check if this cluster exists
			if (reactingMap.find(key) == reactingMap.end())
				continue;

			// Compute nSquare for the dispersion
			nHeSquare += heIndex * heIndex;
			nVSquare += vIndex * vIndex;

			// Get all the reaction vectors at this index
			reactingPairs = reactingMap[key];
			combiningReactants = combiningMap[key];
			dissociatingPairs = dissociatingMap[key];
			emissionPairs = emissionMap[key];

			// Initialize all the effective vectors
			effReactingPairs.clear();
			effCombiningReactants.clear();
			effDissociatingPairs.clear();
			effEmissionPairs.clear();

			// Compute the reaction constant associated to the reacting pairs
			// Set the total number of reacting pairs
			int nPairs = reactingPairs.size();

			// Loop on them
			for (int i = 0; i < nPairs; i++) {
				// Get the reactants
				firstReactant = reactingPairs[i].first;
				secondReactant = reactingPairs[i].second;
				// Compute the reaction constant
				rate = calculateReactionRateConstant(*firstReactant,
						*secondReactant);
				// Set it in the pair
				reactingMap[key][i].kConstant = rate / (double) nTot;

				// Add the reacting pair to the effective vector
				// if the rate is not 0.0
				if (!xolotlCore::equal(rate, 0.0)) {
					effReactingPairs.push_back(&reactingMap[key][i]);

					// Check if the rate is the biggest one up to now
					if (rate > biggestProductionRate)
						biggestProductionRate = rate;
				}
			}

			// Compute the reaction constant associated to the combining reactants
			// Set the total number of combining reactants
			int nReactants = combiningReactants.size();
			// Loop on them
			for (int i = 0; i < nReactants; i++) {
				// Get the reactants
				combiningReactant = combiningReactants[i].combining;
				// Compute the reaction constant
				rate = calculateReactionRateConstant(*this, *combiningReactant);
				// Set it in the combining cluster
				combiningMap[key][i].kConstant = rate / (double) nTot;

				// Add the combining reactant to the effective vector
				// if the rate is not 0.0
				if (!xolotlCore::equal(rate, 0.0)) {
					effCombiningReactants.push_back(&combiningMap[key][i]);

					// Add itself to the list again to account for the correct rate
					if (id == combiningReactant->getId())
						effCombiningReactants.push_back(&combiningMap[key][i]);
				}
			}

			// Compute the dissociation constant associated to the dissociating clusters
			// Set the total number of dissociating clusters
			nPairs = dissociatingPairs.size();
			// Loop on them
			for (int i = 0; i < nPairs; i++) {
				dissociatingCluster = dissociatingPairs[i].first;
				// The second element of the pair is the cluster that is also
				// emitted by the dissociation
				otherEmittedCluster = dissociatingPairs[i].second;
				// Compute the dissociation constant
				// The order of the cluster is important here because of the binding
				// energy used in the computation. It is taken from the type of the first cluster
				// which must be the single one
				// otherEmittedCluster is the single size one
				rate = calculateDissociationConstant(*dissociatingCluster,
						*otherEmittedCluster, *this);

				// Set it in the pair
				dissociatingMap[key][i].kConstant = rate / (double) nTot;

				// Add the dissociating pair to the effective vector
				// if the rate is not 0.0
				if (!xolotlCore::equal(rate, 0.0)) {
					effDissociatingPairs.push_back(&dissociatingMap[key][i]);

					// Add itself to the list again to account for the correct rate
					if (id == otherEmittedCluster->getId())
						effDissociatingPairs.push_back(
								&dissociatingMap[key][i]);
				}
			}

			// Compute the dissociation constant associated to the emission of pairs of clusters
			// Set the total number of emission pairs
			nPairs = emissionPairs.size();
			// Loop on them
			for (int i = 0; i < nPairs; i++) {
				firstCluster = emissionPairs[i].first;
				secondCluster = emissionPairs[i].second;
				// Compute the dissociation rate
				rate = calculateDissociationConstant(*this, *firstCluster,
						*secondCluster);
				// Set it in the pair
				emissionMap[key][i].kConstant = rate / (double) nTot;

				// Add the emission pair to the effective vector
				// if the rate is not 0.0
				if (!xolotlCore::equal(rate, 0.0)) {
					effEmissionPairs.push_back(&emissionMap[key][i]);
				}
			}

			// Shrink the arrays to save some space
			effReactingPairs.shrink_to_fit();
			effCombiningReactants.shrink_to_fit();
			effDissociatingPairs.shrink_to_fit();
			effEmissionPairs.shrink_to_fit();

			// Set the arrays in the effective maps
			effReactingMap[key] = effReactingPairs;
			effCombiningMap[key] = effCombiningReactants;
			effDissociatingMap[key] = effDissociatingPairs;
			effEmissionMap[key] = effEmissionPairs;
		}
	}

	// Reset the vectors to save memory
	reactingPairs.clear();
	combiningReactants.clear();
	dissociatingPairs.clear();
	emissionPairs.clear();

	// Set the biggest rate to the biggest production rate
	biggestRate = biggestProductionRate;

	// Compute the dispersions
	if (sectionHeWidth == 1)
		dispersionHe = 1.0;
	else
		dispersionHe = 2.0
				* ((double) nHeSquare
						- ((double) compositionMap[heType]
								* ((double) compositionMap[heType]
										/ (double) nTot)))
				/ ((double) (nTot * (sectionHeWidth - 1)));

	if (sectionVWidth == 1)
		dispersionV = 1.0;
	else
		dispersionV = 2.0
				* ((double) nVSquare
						- ((double) compositionMap[vType]
								* ((double) compositionMap[vType]
										/ (double) nTot)))
				/ ((double) (nTot * (sectionVWidth - 1)));

	// Method to optimize the reaction vectors
	optimizeReactions();

	return;
}

void SuperCluster::optimizeReactions() {
	// Local declarations
	double heFactor = 0.0, vFactor = 0.0, heDistance = 0.0, vDistance = 0.0;
	int nPairs = 0;
	PSICluster *firstReactant, *secondReactant, *combiningReactant,
			*dissociatingCluster, *otherEmittedCluster, *firstCluster,
			*secondCluster;
	int heIndex = 0, vIndex = 0;

	// Loop on the effective reacting map
	for (auto mapIt = effReactingMap.begin(); mapIt != effReactingMap.end();
			++mapIt) {
		// Get the pairs
		auto pairs = mapIt->second;
		// Loop over all the reacting pairs
		for (auto it = pairs.begin(); it != pairs.end();) {
			// Get the two reacting clusters
			firstReactant = (*it)->first;
			secondReactant = (*it)->second;

			// Create a new SuperClusterProductionPair
			SuperClusterProductionPair superPair(firstReactant, secondReactant,
					(*it)->kConstant);

			// Loop on the whole super cluster to fill this super pair
			for (auto mapItBis = mapIt; mapItBis != effReactingMap.end();
					++mapItBis) {
				// Compute the helium index
				heIndex = mapItBis->first.first;
				heFactor = (double) (heIndex - numHe) / dispersionHe;
				// Compute the vacancy index
				vIndex = mapItBis->first.second;
				vFactor = (double) (vIndex - numV) / dispersionV;

				// Get the pairs
				auto pairsBis = mapItBis->second;
				// Set the total number of reactants that produce to form this one
				// Loop over all the reacting pairs
				for (auto itBis = pairsBis.begin(); itBis != pairsBis.end();) {
					// Get the two reacting clusters
					auto firstReactantBis = (*itBis)->first;
					auto secondReactantBis = (*itBis)->second;

					// Check if it is the same reaction
					if (firstReactantBis == firstReactant
							&& secondReactantBis == secondReactant) {
						// First is A, second is B, in A + B -> this
						superPair.a000 += 1.0;
						superPair.a001 += heFactor;
						superPair.a002 += vFactor;
						superPair.a100 += (*itBis)->firstHeDistance;
						superPair.a101 += (*itBis)->firstHeDistance * heFactor;
						superPair.a102 += (*itBis)->firstHeDistance * vFactor;
						superPair.a200 += (*itBis)->firstVDistance;
						superPair.a201 += (*itBis)->firstVDistance * heFactor;
						superPair.a202 += (*itBis)->firstVDistance * vFactor;
						superPair.a010 += (*itBis)->secondHeDistance;
						superPair.a011 += (*itBis)->secondHeDistance * heFactor;
						superPair.a012 += (*itBis)->secondHeDistance * vFactor;
						superPair.a020 += (*itBis)->secondVDistance;
						superPair.a021 += (*itBis)->secondVDistance * heFactor;
						superPair.a022 += (*itBis)->secondVDistance * vFactor;
						superPair.a110 += (*itBis)->firstHeDistance
								* (*itBis)->secondHeDistance;
						superPair.a111 += (*itBis)->firstHeDistance
								* (*itBis)->secondHeDistance * heFactor;
						superPair.a112 += (*itBis)->firstHeDistance
								* (*itBis)->secondHeDistance * vFactor;
						superPair.a120 += (*itBis)->firstHeDistance
								* (*itBis)->secondVDistance;
						superPair.a121 += (*itBis)->firstHeDistance
								* (*itBis)->secondVDistance * heFactor;
						superPair.a122 += (*itBis)->firstHeDistance
								* (*itBis)->secondVDistance * vFactor;
						superPair.a210 += (*itBis)->firstVDistance
								* (*itBis)->secondHeDistance;
						superPair.a211 += (*itBis)->firstVDistance
								* (*itBis)->secondHeDistance * heFactor;
						superPair.a212 += (*itBis)->firstVDistance
								* (*itBis)->secondHeDistance * vFactor;
						superPair.a220 += (*itBis)->firstVDistance
								* (*itBis)->secondVDistance;
						superPair.a221 += (*itBis)->firstVDistance
								* (*itBis)->secondVDistance * heFactor;
						superPair.a222 += (*itBis)->firstVDistance
								* (*itBis)->secondVDistance * vFactor;

						// Do not delete the element if it is the original one
						if (itBis == it) {
							++itBis;
							continue;
						}

						// Remove the reaction from the vector
						itBis = pairsBis.erase(itBis);
					}
					// Go to the next element
					else
						++itBis;
				}

				// Give back the pairs
				mapItBis->second = pairsBis;
			}

			// Add the super pair
			effReactingList.push_front(superPair);

//			auto firstBounds = superPair.first->getBoundaries();
//			auto secondBounds = superPair.second->getBoundaries();
//			auto myBounds = getBoundaries();
//			std::vector<int> bounds = {firstBounds[0], firstBounds[1], firstBounds[2], firstBounds[3],
//					secondBounds[0], secondBounds[1], secondBounds[2], secondBounds[3],
//					myBounds[0], myBounds[1], myBounds[2], myBounds[3]};
//			std::vector<int> m = {0, 1, 0};
//			double kOut = productGainFactor(bounds, m, 0);
//
//			std::cout << superPair.first->getName() << " + " << superPair.second->getName() << " + " << name <<
//					" : " << superPair.a010 << " " << kOut << std::endl;

			// Remove the reaction from the vector
			it = pairs.erase(it);
		}
	}

	// Loop on the effective combining map
	for (auto mapIt = effCombiningMap.begin(); mapIt != effCombiningMap.end();
			++mapIt) {
		// Get the pairs
		auto clusters = mapIt->second;
		// Loop over all the reacting pairs
		for (auto it = clusters.begin(); it != clusters.end();) {
			// Get the combining cluster
			combiningReactant = (*it)->combining;

			// Create a new SuperClusterProductionPair with NULL as the second cluster because
			// we do not need it
			SuperClusterProductionPair superPair(combiningReactant, NULL,
					(*it)->kConstant);

			// Loop on the whole super cluster to fill this super pair
			for (auto mapItBis = mapIt; mapItBis != effCombiningMap.end();
					++mapItBis) {
				// Compute the helium index
				heIndex = mapItBis->first.first;
				heDistance = getHeDistance(heIndex);
				heFactor = (double) (heIndex - numHe) / dispersionHe;
				// Compute the vacancy index
				vIndex = mapItBis->first.second;
				vDistance = getVDistance(vIndex);
				vFactor = (double) (vIndex - numV) / dispersionV;

				// Get the pairs
				auto clustersBis = mapItBis->second;
				// Set the total number of reactants that produce to form this one
				// Loop over all the reacting pairs
				for (auto itBis = clustersBis.begin();
						itBis != clustersBis.end();) {
					// Get the two reacting clusters
					auto combiningReactantBis = (*itBis)->combining;

					// Check if it is the same reaction
					if (combiningReactantBis == combiningReactant) {
						// This is A, itBis is B, in this + B -> C
						superPair.a000 += 1.0;
						superPair.a001 += heFactor;
						superPair.a002 += vFactor;
						superPair.a010 += (*itBis)->heDistance;
						superPair.a011 += (*itBis)->heDistance * heFactor;
						superPair.a012 += (*itBis)->heDistance * vFactor;
						superPair.a020 += (*itBis)->vDistance;
						superPair.a021 += (*itBis)->vDistance * heFactor;
						superPair.a022 += (*itBis)->vDistance * vFactor;
						superPair.a100 += heDistance;
						superPair.a101 += heDistance * heFactor;
						superPair.a102 += heDistance * vFactor;
						superPair.a200 += vDistance;
						superPair.a201 += vDistance * heFactor;
						superPair.a202 += vDistance * vFactor;
						superPair.a110 += (*itBis)->heDistance * heDistance;
						superPair.a111 += (*itBis)->heDistance * heDistance
								* heFactor;
						superPair.a112 += (*itBis)->heDistance * heDistance
								* vFactor;
						superPair.a210 += (*itBis)->heDistance * vDistance;
						superPair.a211 += (*itBis)->heDistance * vDistance
								* heFactor;
						superPair.a212 += (*itBis)->heDistance * vDistance
								* vFactor;
						superPair.a120 += (*itBis)->vDistance * heDistance;
						superPair.a121 += (*itBis)->vDistance * heDistance
								* heFactor;
						superPair.a122 += (*itBis)->vDistance * heDistance
								* vFactor;
						superPair.a220 += (*itBis)->vDistance * vDistance;
						superPair.a221 += (*itBis)->vDistance * vDistance
								* heFactor;
						superPair.a222 += (*itBis)->vDistance * vDistance
								* vFactor;

						// Do not delete the element if it is the original one
						if (itBis == it) {
							++itBis;
							continue;
						}

						// Remove the reaction from the vector
						itBis = clustersBis.erase(itBis);
					}
					// Go to the next element
					else
						++itBis;
				}

				// Give back the pairs
				mapItBis->second = clustersBis;
			}

			// Add the super pair
			effCombiningList.push_front(superPair);

			// Remove the reaction from the vector
			it = clusters.erase(it);
		}
	}

	// Loop on the effective dissociating map
	for (auto mapIt = effDissociatingMap.begin();
			mapIt != effDissociatingMap.end(); ++mapIt) {
		// Get the pairs
		auto pairs = mapIt->second;
		// Loop over all the reacting pairs
		for (auto it = pairs.begin(); it != pairs.end();) {
			// Get the two reacting clusters
			dissociatingCluster = (*it)->first;
			otherEmittedCluster = (*it)->second;

			// Create a new SuperClusterProductionPair
			SuperClusterDissociationPair superPair(dissociatingCluster,
					otherEmittedCluster, (*it)->kConstant);

			// Loop on the whole super cluster to fill this super pair
			for (auto mapItBis = mapIt; mapItBis != effDissociatingMap.end();
					++mapItBis) {
				// Compute the helium index
				heIndex = mapItBis->first.first;
				heFactor = (double) (heIndex - numHe) / dispersionHe;
				// Compute the vacancy index
				vIndex = mapItBis->first.second;
				vFactor = (double) (vIndex - numV) / dispersionV;

				// Get the pairs
				auto pairsBis = mapItBis->second;
				// Set the total number of reactants that produce to form this one
				// Loop over all the reacting pairs
				for (auto itBis = pairsBis.begin(); itBis != pairsBis.end();) {
					// Get the two reacting clusters
					auto dissociatingClusterBis = (*itBis)->first;
					auto otherEmittedClusterBis = (*itBis)->second;

					// Check if it is the same reaction
					if (dissociatingClusterBis == dissociatingCluster
							&& otherEmittedClusterBis == otherEmittedCluster) {
						// A is the dissociating cluster
						superPair.a00 += 1.0;
						superPair.a01 += heFactor;
						superPair.a02 += vFactor;
						superPair.a10 += (*itBis)->firstHeDistance;
						superPair.a11 += (*itBis)->firstHeDistance * heFactor;
						superPair.a12 += (*itBis)->firstHeDistance * vFactor;
						superPair.a20 += (*itBis)->firstVDistance;
						superPair.a21 += (*itBis)->firstVDistance * heFactor;
						superPair.a22 += (*itBis)->firstVDistance * vFactor;

						// Do not delete the element if it is the original one
						if (itBis == it) {
							++itBis;
							continue;
						}

						// Remove the reaction from the vector
						itBis = pairsBis.erase(itBis);
					}
					// Go to the next element
					else
						++itBis;
				}

				// Give back the pairs
				mapItBis->second = pairsBis;
			}

			// Add the super pair
			effDissociatingList.push_front(superPair);

			// Remove the reaction from the vector
			it = pairs.erase(it);
		}
	}

	// Loop on the effective emission map
	for (auto mapIt = effEmissionMap.begin(); mapIt != effEmissionMap.end();
			++mapIt) {
		// Get the pairs
		auto pairs = mapIt->second;
		// Loop over all the reacting pairs
		for (auto it = pairs.begin(); it != pairs.end();) {
			// Get the two reacting clusters
			firstCluster = (*it)->first;
			secondCluster = (*it)->second;

			// Create a new SuperClusterProductionPair
			SuperClusterDissociationPair superPair(firstCluster, secondCluster,
					(*it)->kConstant);

			// Loop on the whole super cluster to fill this super pair
			for (auto mapItBis = mapIt; mapItBis != effEmissionMap.end();
					++mapItBis) {
				// Compute the helium index
				heIndex = mapItBis->first.first;
				heDistance = getHeDistance(heIndex);
				heFactor = (double) (heIndex - numHe) / dispersionHe;
				// Compute the vacancy index
				vIndex = mapItBis->first.second;
				vDistance = getVDistance(vIndex);
				vFactor = (double) (vIndex - numV) / dispersionV;

				// Get the pairs
				auto pairsBis = mapItBis->second;
				// Set the total number of reactants that produce to form this one
				// Loop over all the reacting pairs
				for (auto itBis = pairsBis.begin(); itBis != pairsBis.end();) {
					// Get the two reacting clusters
					auto firstClusterBis = (*itBis)->first;
					auto secondClusterBis = (*itBis)->second;

					// Check if it is the same reaction
					if (firstClusterBis == firstCluster
							&& secondClusterBis == secondCluster) {
						// A is the dissociating cluster
						superPair.a00 += 1.0;
						superPair.a01 += heFactor;
						superPair.a02 += vFactor;
						superPair.a10 += heDistance;
						superPair.a11 += heDistance * heFactor;
						superPair.a12 += heDistance * vFactor;
						superPair.a20 += vDistance;
						superPair.a21 += vDistance * heFactor;
						superPair.a22 += vDistance * vFactor;

						// Do not delete the element if it is the original one
						if (itBis == it) {
							++itBis;
							continue;
						}

						// Remove the reaction from the vector
						itBis = pairsBis.erase(itBis);
					}
					// Go to the next element
					else
						++itBis;
				}

				// Give back the pairs
				mapItBis->second = pairsBis;
			}

			// Add the super pair
			effEmissionList.push_front(superPair);

			// Remove the reaction from the vector
			it = pairs.erase(it);
		}
	}

	// Clear the maps because they won't be used anymore
	effReactingPairs.clear();
	effCombiningReactants.clear();
	effDissociatingPairs.clear();
	effEmissionPairs.clear();
	reactingPairs.clear();
	combiningReactants.clear();
	dissociatingPairs.clear();
	emissionPairs.clear();

	return;
}

void SuperCluster::updateRateConstants() {
	// Local declarations
	PSICluster *firstReactant, *secondReactant, *combiningReactant,
			*dissociatingCluster, *otherEmittedCluster, *firstCluster,
			*secondCluster;
	double rate = 0.0;
	// Initialize the value for the biggest production rate
	double biggestProductionRate = 0.0;

	// Loop on the reacting list
	for (auto it = effReactingList.begin(); it != effReactingList.end(); ++it) {
		// Get the reactants
		firstReactant = (*it).first;
		secondReactant = (*it).second;
		// Compute the reaction constant
		rate = calculateReactionRateConstant(*firstReactant, *secondReactant);
		// Set it in the pair
		(*it).kConstant = rate / (double) nTot;

		// Check if the rate is the biggest one up to now
		if (rate > biggestProductionRate)
			biggestProductionRate = rate;
	}

	// Loop on the combining list
	for (auto it = effCombiningList.begin(); it != effCombiningList.end();
			++it) {
		// Get the reactants
		combiningReactant = (*it).first;
		// Compute the reaction constant
		rate = calculateReactionRateConstant(*this, *combiningReactant);
		// Set it in the combining cluster
		(*it).kConstant = rate / (double) nTot;
	}

	// Loop on the dissociating list
	for (auto it = effDissociatingList.begin(); it != effDissociatingList.end();
			++it) {
		dissociatingCluster = (*it).first;
		// The second element of the pair is the cluster that is also
		// emitted by the dissociation
		otherEmittedCluster = (*it).second;
		// Compute the dissociation constant
		// The order of the cluster is important here because of the binding
		// energy used in the computation. It is taken from the type of the first cluster
		// which must be the single one
		// otherEmittedCluster is the single size one
		rate = calculateDissociationConstant(*dissociatingCluster,
				*otherEmittedCluster, *this);
		// Set it in the pair
		(*it).kConstant = rate / (double) nTot;
	}

	// Loop on the emission list
	for (auto it = effEmissionList.begin(); it != effEmissionList.end(); ++it) {
		firstCluster = (*it).first;
		secondCluster = (*it).second;
		// Compute the dissociation rate
		rate = calculateDissociationConstant(*this, *firstCluster,
				*secondCluster);
		// Set it in the pair
		(*it).kConstant = rate / (double) nTot;
	}

	// Set the biggest rate to the biggest production rate
	biggestRate = biggestProductionRate;

	return;
}

void SuperCluster::resetConnectivities() {
	// Clear both sets
	reactionConnectivitySet.clear();
	dissociationConnectivitySet.clear();

	// Connect this cluster to itself since any reaction will affect it
	setReactionConnectivity(id);
	setDissociationConnectivity(id);
	setReactionConnectivity(heMomId);
	setDissociationConnectivity(heMomId);
	setReactionConnectivity(vMomId);
	setDissociationConnectivity(vMomId);

	// Loop over all the reacting pairs
	for (auto it = effReactingList.begin(); it != effReactingList.end(); ++it) {
		// The cluster is connecting to both clusters in the pair
		setReactionConnectivity((*it).first->getId());
		setReactionConnectivity((*it).first->getHeMomentumId());
		setReactionConnectivity((*it).first->getVMomentumId());
		setReactionConnectivity((*it).second->getId());
		setReactionConnectivity((*it).second->getHeMomentumId());
		setReactionConnectivity((*it).second->getVMomentumId());
	}

	// Loop over all the combining pairs
	for (auto it = effCombiningList.begin(); it != effCombiningList.end();
			++it) {
		// The cluster is connecting to the combining cluster
		setReactionConnectivity((*it).first->getId());
		setReactionConnectivity((*it).first->getHeMomentumId());
		setReactionConnectivity((*it).first->getVMomentumId());
	}

	// Loop over all the dissociating pairs
	for (auto it = effDissociatingList.begin(); it != effDissociatingList.end();
			++it) {
		// The cluster is connecting to the combining cluster
		setDissociationConnectivity((*it).first->getId());
		setDissociationConnectivity((*it).first->getHeMomentumId());
		setDissociationConnectivity((*it).first->getVMomentumId());
	}

	// Don't loop on the effective emission pairs because
	// this cluster is not connected to them

	// Initialize the partial vector for the momentum
	int dof = network->size() + 2 * network->getAll(superType).size();
	heMomentumPartials.resize(dof, 0.0);
	vMomentumPartials.resize(dof, 0.0);

	return;
}

double SuperCluster::getDissociationFlux() {
	// Initial declarations
	double flux = 0.0, value = 0.0;
	PSICluster *dissociatingCluster;

	// Loop over all the dissociating pairs
	for (auto it = effDissociatingList.begin(); it != effDissociatingList.end();
			++it) {
		// Get the dissociating clusters
		dissociatingCluster = (*it).first;
		double l0A = dissociatingCluster->getConcentration(0.0, 0.0);
		double lHeA = dissociatingCluster->getHeMomentum();
		double lVA = dissociatingCluster->getVMomentum();
		// Update the flux
		value = (*it).kConstant;
		flux += value * ((*it).a00 * l0A + (*it).a10 * lHeA + (*it).a20 * lVA);
		// Compute the momentum fluxes
		heMomentumFlux += value
				* ((*it).a01 * l0A + (*it).a11 * lHeA + (*it).a21 * lVA);
		vMomentumFlux += value
				* ((*it).a02 * l0A + (*it).a12 * lHeA + (*it).a22 * lVA);
	}

	// Return the flux
	return flux;
}

double SuperCluster::getEmissionFlux() {
	// Initial declarations
	double flux = 0.0, value = 0.0;

	// Loop over all the emission pairs
	for (auto it = effEmissionList.begin(); it != effEmissionList.end(); ++it) {
		// Update the flux
		value = (*it).kConstant;
		flux += value * ((*it).a00 * l0 + (*it).a10 * l1He + (*it).a20 * l1V);
		// Compute the momentum fluxes
		heMomentumFlux -= value
				* ((*it).a01 * l0 + (*it).a11 * l1He + (*it).a21 * l1V);
		vMomentumFlux -= value
				* ((*it).a02 * l0 + (*it).a12 * l1He + (*it).a22 * l1V);
	}

	return flux;
}

double SuperCluster::getProductionFlux() {
	// Local declarations
	double flux = 0.0, value = 0.0;
	PSICluster *firstReactant, *secondReactant;

	// Loop over all the reacting pairs
	for (auto it = effReactingList.begin(); it != effReactingList.end(); ++it) {
		// Get the two reacting clusters
		firstReactant = (*it).first;
		secondReactant = (*it).second;
		double l0A = firstReactant->getConcentration(0.0, 0.0);
		double l0B = secondReactant->getConcentration(0.0, 0.0);
		double lHeA = firstReactant->getHeMomentum();
		double lHeB = secondReactant->getHeMomentum();
		double lVA = firstReactant->getVMomentum();
		double lVB = secondReactant->getVMomentum();
		// Update the flux
		value = (*it).kConstant;
		flux += value
				* ((*it).a000 * l0A * l0B + (*it).a010 * l0A * lHeB
						+ (*it).a020 * l0A * lVB + (*it).a100 * lHeA * l0B
						+ (*it).a110 * lHeA * lHeB + (*it).a120 * lHeA * lVB
						+ (*it).a200 * lVA * l0B + (*it).a210 * lVA * lHeB
						+ (*it).a220 * lVA * lVB);
		// Compute the momentum fluxes
		heMomentumFlux += value
				* ((*it).a001 * l0A * l0B + (*it).a011 * l0A * lHeB
						+ (*it).a021 * l0A * lVB + (*it).a101 * lHeA * l0B
						+ (*it).a111 * lHeA * lHeB + (*it).a121 * lHeA * lVB
						+ (*it).a201 * lVA * l0B + (*it).a211 * lVA * lHeB
						+ (*it).a221 * lVA * lVB);
		vMomentumFlux += value
				* ((*it).a002 * l0A * l0B + (*it).a012 * l0A * lHeB
						+ (*it).a022 * l0A * lVB + (*it).a102 * lHeA * l0B
						+ (*it).a112 * lHeA * lHeB + (*it).a122 * lHeA * lVB
						+ (*it).a202 * lVA * l0B + (*it).a212 * lVA * lHeB
						+ (*it).a222 * lVA * lVB);
	}

	// Return the production flux
	return flux;
}

double SuperCluster::getCombinationFlux() {
	// Local declarations
	double flux = 0.0, value = 0.0;
	int nReactants = 0;
	PSICluster *combiningCluster;

	// Loop over all the combining clusters
	for (auto it = effCombiningList.begin(); it != effCombiningList.end();
			++it) {
		// Get the two reacting clusters
		combiningCluster = (*it).first;
		double l0B = combiningCluster->getConcentration(0.0, 0.0);
		double lHeB = combiningCluster->getHeMomentum();
		double lVB = combiningCluster->getVMomentum();
		// Update the flux
		value = (*it).kConstant;
		flux += value
				* ((*it).a000 * l0B * l0 + (*it).a100 * l0B * l1He
						+ (*it).a200 * l0B * l1V + (*it).a010 * lHeB * l0
						+ (*it).a110 * lHeB * l1He + (*it).a210 * lHeB * l1V
						+ (*it).a020 * lVB * l0 + (*it).a120 * lVB * l1He
						+ (*it).a220 * lVB * l1V);
		// Compute the momentum fluxes
		heMomentumFlux -= value
				* ((*it).a001 * l0B * l0 + (*it).a101 * l0B * l1He
						+ (*it).a201 * l0B * l1V + (*it).a011 * lHeB * l0
						+ (*it).a111 * lHeB * l1He + (*it).a211 * lHeB * l1V
						+ (*it).a021 * lVB * l0 + (*it).a121 * lVB * l1He
						+ (*it).a221 * lVB * l1V);
		vMomentumFlux -= value
				* ((*it).a002 * l0B * l0 + (*it).a102 * l0B * l1He
						+ (*it).a202 * l0B * l1V + (*it).a012 * lHeB * l0
						+ (*it).a112 * lHeB * l1He + (*it).a212 * lHeB * l1V
						+ (*it).a022 * lVB * l0 + (*it).a122 * lVB * l1He
						+ (*it).a222 * lVB * l1V);
	}

	return flux;
}

void SuperCluster::getPartialDerivatives(std::vector<double> & partials) const {
	// Reinitialize the momentum partial derivatives vector
	std::fill(heMomentumPartials.begin(), heMomentumPartials.end(), 0.0);
	std::fill(vMomentumPartials.begin(), vMomentumPartials.end(), 0.0);

	// Get the partial derivatives for each reaction type
	getProductionPartialDerivatives(partials);
	getCombinationPartialDerivatives(partials);
	getDissociationPartialDerivatives(partials);
	getEmissionPartialDerivatives(partials);

	return;
}

void SuperCluster::getProductionPartialDerivatives(
		std::vector<double> & partials) const {
	// Initial declarations
	double value = 0.0;
	int index = 0;
	PSICluster *firstReactant, *secondReactant;

	// Production
	// A + B --> D, D being this cluster
	// The flux for D is
	// F(C_D) = k+_(A,B)*C_A*C_B
	// Thus, the partial derivatives
	// dF(C_D)/dC_A = k+_(A,B)*C_B
	// dF(C_D)/dC_B = k+_(A,B)*C_A

	// Loop over all the reacting pairs
	for (auto it = effReactingList.begin(); it != effReactingList.end(); ++it) {
		// Get the two reacting clusters
		firstReactant = (*it).first;
		secondReactant = (*it).second;
		double l0A = firstReactant->getConcentration(0.0, 0.0);
		double l0B = secondReactant->getConcentration(0.0, 0.0);
		double lHeA = firstReactant->getHeMomentum();
		double lHeB = secondReactant->getHeMomentum();
		double lVA = firstReactant->getVMomentum();
		double lVB = secondReactant->getVMomentum();

		// Compute the contribution from the first part of the reacting pair
		value = (*it).kConstant;
		index = firstReactant->getId() - 1;
		partials[index] += value
				* ((*it).a000 * l0B + (*it).a010 * lHeB + (*it).a020 * lVB);
		heMomentumPartials[index] += value
				* ((*it).a001 * l0B + (*it).a011 * lHeB + (*it).a021 * lVB);
		vMomentumPartials[index] += value
				* ((*it).a002 * l0B + (*it).a012 * lHeB + (*it).a022 * lVB);
		index = firstReactant->getHeMomentumId() - 1;
		partials[index] += value
				* ((*it).a100 * l0B + (*it).a110 * lHeB + (*it).a120 * lVB);
		heMomentumPartials[index] += value
				* ((*it).a101 * l0B + (*it).a111 * lHeB + (*it).a121 * lVB);
		vMomentumPartials[index] += value
				* ((*it).a102 * l0B + (*it).a112 * lHeB + (*it).a122 * lVB);
		index = firstReactant->getVMomentumId() - 1;
		partials[index] += value
				* ((*it).a200 * l0B + (*it).a210 * lHeB + (*it).a220 * lVB);
		heMomentumPartials[index] += value
				* ((*it).a201 * l0B + (*it).a211 * lHeB + (*it).a221 * lVB);
		vMomentumPartials[index] += value
				* ((*it).a202 * l0B + (*it).a212 * lHeB + (*it).a222 * lVB);
		// Compute the contribution from the second part of the reacting pair
		index = secondReactant->getId() - 1;
		partials[index] += value
				* ((*it).a000 * l0A + (*it).a100 * lHeA + (*it).a200 * lVA);
		heMomentumPartials[index] += value
				* ((*it).a001 * l0A + (*it).a101 * lHeA + (*it).a201 * lVA);
		vMomentumPartials[index] += value
				* ((*it).a002 * l0A + (*it).a102 * lHeA + (*it).a202 * lVA);
		index = secondReactant->getHeMomentumId() - 1;
		partials[index] += value
				* ((*it).a010 * l0A + (*it).a110 * lHeA + (*it).a210 * lVA);
		heMomentumPartials[index] += value
				* ((*it).a011 * l0A + (*it).a111 * lHeA + (*it).a211 * lVA);
		vMomentumPartials[index] += value
				* ((*it).a012 * l0A + (*it).a112 * lHeA + (*it).a212 * lVA);
		index = secondReactant->getVMomentumId() - 1;
		partials[index] += value
				* ((*it).a020 * l0A + (*it).a120 * lHeA + (*it).a220 * lVA);
		heMomentumPartials[index] += value
				* ((*it).a021 * l0A + (*it).a121 * lHeA + (*it).a221 * lVA);
		vMomentumPartials[index] += value
				* ((*it).a022 * l0A + (*it).a122 * lHeA + (*it).a222 * lVA);
	}

	return;
}

void SuperCluster::getCombinationPartialDerivatives(
		std::vector<double> & partials) const {
	// Initial declarations
	int index = 0;
	PSICluster *cluster;
	double value = 0.0;

	// Combination
	// A + B --> D, A being this cluster
	// The flux for A is outgoing
	// F(C_A) = - k+_(A,B)*C_A*C_B
	// Thus, the partial derivatives
	// dF(C_A)/dC_A = - k+_(A,B)*C_B
	// dF(C_A)/dC_B = - k+_(A,B)*C_A

	// Loop over all the combining clusters
	for (auto it = effCombiningList.begin(); it != effCombiningList.end();
			++it) {
		// Get the two reacting clusters
		cluster = (*it).first;
		double l0B = cluster->getConcentration(0.0, 0.0);
		double lHeB = cluster->getHeMomentum();
		double lVB = cluster->getVMomentum();

		// Compute the contribution from the combining cluster
		value = (*it).kConstant;
		index = cluster->getId() - 1;
		partials[index] -= value
				* ((*it).a000 * l0 + (*it).a100 * l1He + (*it).a200 * l1V);
		heMomentumPartials[index] -= value
				* ((*it).a001 * l0 + (*it).a101 * l1He + (*it).a201 * l1V);
		vMomentumPartials[index] -= value
				* ((*it).a002 * l0 + (*it).a102 * l1He + (*it).a202 * l1V);
		index = cluster->getHeMomentumId() - 1;
		partials[index] -= value
				* ((*it).a010 * l0 + (*it).a110 * l1He + (*it).a210 * l1V);
		heMomentumPartials[index] -= value
				* ((*it).a011 * l0 + (*it).a111 * l1He + (*it).a211 * l1V);
		vMomentumPartials[index] -= value
				* ((*it).a012 * l0 + (*it).a112 * l1He + (*it).a212 * l1V);
		index = cluster->getVMomentumId() - 1;
		partials[index] -= value
				* ((*it).a020 * l0 + (*it).a120 * l1He + (*it).a220 * l1V);
		heMomentumPartials[index] -= value
				* ((*it).a021 * l0 + (*it).a121 * l1He + (*it).a221 * l1V);
		vMomentumPartials[index] -= value
				* ((*it).a022 * l0 + (*it).a122 * l1He + (*it).a222 * l1V);
		// Compute the contribution from this cluster
		index = id - 1;
		partials[index] -= value
				* ((*it).a000 * l0B + (*it).a010 * lHeB + (*it).a020 * lVB);
		heMomentumPartials[index] -= value
				* ((*it).a001 * l0B + (*it).a011 * lHeB + (*it).a021 * lVB);
		vMomentumPartials[index] -= value
				* ((*it).a002 * l0B + (*it).a012 * lHeB + (*it).a022 * lVB);
		index = heMomId - 1;
		partials[index] -= value
				* ((*it).a100 * l0B + (*it).a110 * lHeB + (*it).a120 * lVB);
		heMomentumPartials[index] -= value
				* ((*it).a101 * l0B + (*it).a111 * lHeB + (*it).a121 * lVB);
		vMomentumPartials[index] -= value
				* ((*it).a102 * l0B + (*it).a112 * lHeB + (*it).a122 * lVB);
		index = vMomId - 1;
		partials[index] -= value
				* ((*it).a200 * l0B + (*it).a210 * lHeB + (*it).a220 * lVB);
		heMomentumPartials[index] -= value
				* ((*it).a201 * l0B + (*it).a211 * lHeB + (*it).a221 * lVB);
		vMomentumPartials[index] -= value
				* ((*it).a202 * l0B + (*it).a212 * lHeB + (*it).a222 * lVB);
	}

	return;
}

void SuperCluster::getDissociationPartialDerivatives(
		std::vector<double> & partials) const {
	// Initial declarations
	int index = 0;
	PSICluster *cluster;
	double value = 0.0;

	// Dissociation
	// A --> B + D, B being this cluster
	// The flux for B is
	// F(C_B) = k-_(B,D)*C_A
	// Thus, the partial derivatives
	// dF(C_B)/dC_A = k-_(B,D)

	// Loop over all the dissociating pairs
	for (auto it = effDissociatingList.begin(); it != effDissociatingList.end();
			++it) {
		// Get the dissociating clusters
		cluster = (*it).first;
		// Compute the contribution from the dissociating cluster
		value = (*it).kConstant;
		index = cluster->getId() - 1;
		partials[index] += value * ((*it).a00);
		heMomentumPartials[index] += value * ((*it).a01);
		vMomentumPartials[index] += value * ((*it).a02);
		index = cluster->getHeMomentumId() - 1;
		partials[index] += value * ((*it).a10);
		heMomentumPartials[index] += value * ((*it).a11);
		vMomentumPartials[index] += value * ((*it).a12);
		index = cluster->getVMomentumId() - 1;
		partials[index] += value * ((*it).a20);
		heMomentumPartials[index] += value * ((*it).a21);
		vMomentumPartials[index] += value * ((*it).a22);
	}

	return;
}

void SuperCluster::getEmissionPartialDerivatives(
		std::vector<double> & partials) const {
	// Initial declarations
	int index = 0;
	double value = 0.0;

	// Emission
	// A --> B + D, A being this cluster
	// The flux for A is
	// F(C_A) = - k-_(B,D)*C_A
	// Thus, the partial derivatives
	// dF(C_A)/dC_A = - k-_(B,D)

	// Loop over all the emission pairs
	for (auto it = effEmissionList.begin(); it != effEmissionList.end(); ++it) {
		// Compute the contribution from the dissociating cluster
		value = (*it).kConstant;
		index = id - 1;
		partials[index] -= value * ((*it).a00);
		heMomentumPartials[index] -= value * ((*it).a01);
		vMomentumPartials[index] -= value * ((*it).a02);
		index = heMomId - 1;
		partials[index] -= value * ((*it).a10);
		heMomentumPartials[index] -= value * ((*it).a11);
		vMomentumPartials[index] -= value * ((*it).a12);
		index = vMomId - 1;
		partials[index] -= value * ((*it).a20);
		heMomentumPartials[index] -= value * ((*it).a21);
		vMomentumPartials[index] -= value * ((*it).a22);
	}

	return;
}

void SuperCluster::getHeMomentPartialDerivatives(
		std::vector<double> & partials) const {
	// Loop on the size of the vector
	for (int i = 0; i < partials.size(); i++) {
		// Set to the values that were already computed
		partials[i] = heMomentumPartials[i];
	}

	return;
}

void SuperCluster::getVMomentPartialDerivatives(
		std::vector<double> & partials) const {
	// Loop on the size of the vector
	for (int i = 0; i < partials.size(); i++) {
		// Set to the values that were already computed
		partials[i] = vMomentumPartials[i];
	}

	return;
}

double SuperCluster::reactantLossFactor(std::vector<int> bounds,
		std::vector<int> m, int active) {
	// Initial declarations
	std::vector<int> luBounds = { 0, 0, 0, 0, 0, 0, 0, 0 };
	std::vector<double> group = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	// Compute the index related to the He moments
	int p = computeIndex(m[0], m[1], m[2], 1);
	// Set the reaction limits in the He direction
	setReactLimits(bounds[0], bounds[1], bounds[4], bounds[5], bounds[8],
			bounds[9], luBounds, group);
	// Get the first part of the rate
	double kOut = analyticReact(p, group[0], group[1], group[2], group[3],
			group[4], group[5], luBounds, active);
	// Compute the index related to the V moments
	p = computeIndex(m[0], m[1], m[2], 2);
	// Set the reaction limits in the V direction
	setReactLimits(bounds[2], bounds[3], bounds[6], bounds[7], bounds[10],
			bounds[11], luBounds, group);
	// Get the last part of the rate
	kOut = kOut
			* analyticReact(p, group[0], group[1], group[2], group[3], group[4],
					group[5], luBounds, active);

	return kOut;
}

double SuperCluster::productGainFactor(std::vector<int> bounds,
		std::vector<int> m, int active) {
	// Initial declarations
	std::vector<int> luBounds = { 0, 0, 0, 0, 0, 0, 0, 0 };
	std::vector<double> group = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	// Compute the index related to the He moments
	int p = computeIndex(m[0], m[1], m[2], 1);
	// Set the reaction limits in the He direction
	setReactLimits(bounds[0], bounds[1], bounds[4], bounds[5], bounds[8],
			bounds[9], luBounds, group);
	// Get the first part of the rate
	double kOut = analyticProduct(p, group[0], group[1], group[2], group[3],
			group[4], group[5], luBounds, active);
	// Compute the index related to the V moments
	p = computeIndex(m[0], m[1], m[2], 2);
	// Set the reaction limits in the V direction
	setReactLimits(bounds[2], bounds[3], bounds[6], bounds[7], bounds[10],
			bounds[11], luBounds, group);
	// Get the last part of the rate
	kOut = kOut
			* analyticProduct(p, group[0], group[1], group[2], group[3],
					group[4], group[5], luBounds, active);

	return kOut;
}

double SuperCluster::analyticEmitFactor(std::vector<int> bounds,
		std::vector<int> m, int bHe, int bVac) {
	// Initial declarations
	std::vector<int> luBounds = { 0, 0 };
	std::vector<double> group = { 0.0, 0.0, 0.0, 0.0 };
	// Compute the index related to the He moments
	int p = computeIndex(m[0], m[1], 0, 1);
	// Set the emission limits in the He direction
	setEmitLimits(bounds[0], bounds[1], bounds[4], bounds[5], bHe, luBounds,
			group);
	// Get the first part of the rate
	double kOut = ae(p, group[0], bHe, group[1], group[2], 0, group[3],
			luBounds[0], luBounds[1]);
	// Compute the index related to the V moments
	p = computeIndex(m[0], m[1], 0, 2);
	// Set the emission limits in the He direction
	setEmitLimits(bounds[2], bounds[3], bounds[6], bounds[7], bHe, luBounds,
			group);
	// Get the last part of the rate
	kOut = kOut
			* ae(p, group[0], bVac, group[1], group[2], 0, group[3],
					luBounds[0], luBounds[1]);

	return kOut;
}

double SuperCluster::analyticMonoFactor(std::vector<int> bounds,
		std::vector<int> m, int bHe, int bVac) {
	// Initial declarations
	std::vector<int> luBounds = { 0, 0 };
	std::vector<double> group = { 0.0, 0.0, 0.0, 0.0 };
	// Compute the index related to the He moments
	int p = computeIndex(m[0], m[1], 0, 1);
	// Set the emission limits in the He direction
	setEmitLimits(bounds[0], bounds[1], bounds[4], bounds[5], bHe, luBounds,
			group);
	// Get the first part of the rate
	double kOut = am(p, group[0], bHe, group[1], group[2], 0, group[3],
			luBounds[0], luBounds[1]);
	// Compute the index related to the V moments
	p = computeIndex(m[0], m[1], 0, 2);
	// Set the emission limits in the He direction
	setEmitLimits(bounds[2], bounds[3], bounds[6], bounds[7], bHe, luBounds,
			group);
	// Get the last part of the rate
	kOut = kOut
			* am(p, group[0], bVac, group[1], group[2], 0, group[3],
					luBounds[0], luBounds[1]);

	return kOut;
}

double SuperCluster::analyticDaughterFactor(std::vector<int> bounds,
		std::vector<int> m, int bHe, int bVac) {
	// Initial declarations
	std::vector<int> luBounds = { 0, 0 };
	std::vector<double> group = { 0.0, 0.0, 0.0, 0.0 };
	// Compute the index related to the He moments
	int p = computeIndex(m[0], m[1], 0, 1);
	// Set the emission limits in the He direction
	setEmitLimits(bounds[0], bounds[1], bounds[4], bounds[5], bHe, luBounds,
			group);
	// Get the first part of the rate
	double kOut = ad(p, group[0], bHe, group[1], group[2], 0, group[3],
			luBounds[0], luBounds[1]);
	// Compute the index related to the V moments
	p = computeIndex(m[0], m[1], 0, 2);
	// Set the emission limits in the He direction
	setEmitLimits(bounds[2], bounds[3], bounds[6], bounds[7], bHe, luBounds,
			group);
	// Get the last part of the rate
	kOut = kOut
			* ad(p, group[0], bVac, group[1], group[2], 0, group[3],
					luBounds[0], luBounds[1]);

	return kOut;
}

void SuperCluster::setReactLimits(int aMin, int aMax, int bMin, int bMax,
		int cMin, int cMax, std::vector<int> luBounds,
		std::vector<double> group) {
	// Initial declarations
	int lA = 0, uA = 0, i = 0, xA = 0, xD = 0, xL = 0, xH = 0;
	// Compute centers(s0) and widths(w) of each group
	group[0] = 0.5 * (aMax + aMin);
	group[1] = 0.5 * (bMax + bMin);
	group[2] = 0.5 * (cMax + cMin);
	group[3] = 0.5 * (aMax - aMin);
	group[4] = 0.5 * (bMax - bMin);
	group[5] = 0.5 * (cMax - cMin);
	// compute the limits on each of the four summation conditions
	xA = cMin - bMin;
	xD = cMax - bMax;
	xL = cMin - bMax;
	xH = cMax - bMin;
	lA = aMin;
	if (xL > lA)
		lA = xL;
	uA = aMax;
	if (xH < uA)
		uA = xH;
	for (int i = 0; i <= 3; i++) {
		luBounds[i] = lA;
		luBounds[i + 4] = uA;
	}
	if ((xA - 1) < luBounds[4])
		luBounds[4] = xA - 1;
	if ((xD - 1) < luBounds[4])
		luBounds[4] = xD - 1;
	if ((xA + 1) > luBounds[3])
		luBounds[3] = xA + 1;
	if ((xD + 1) > luBounds[3])
		luBounds[3] = xD + 1;
	if ((xA) > luBounds[1])
		luBounds[1] = xA;
	if ((xD) < luBounds[5])
		luBounds[5] = xD;
	if ((xD) > luBounds[2])
		luBounds[2] = xD;
	if ((xA) < luBounds[6])
		luBounds[6] = xA;
	if ((luBounds[2] == luBounds[6]) && (luBounds[1] == luBounds[5]))
		luBounds[6] = luBounds[6] - 1;

	return;
}

void SuperCluster::setEmitLimits(int aMin, int aMax, int cMin, int cMax, int b0,
		std::vector<int> luBounds, std::vector<double> group) {
	// Initial declarations
	int lA = 0, uA = 0;
	group[0] = 0.5 * (aMax + aMin);
	group[1] = b0;
	group[2] = 0.5 * (cMax + cMin);
	group[3] = 0.5 * (aMax - aMin);
	group[4] = 0.0;
	group[5] = 0.5 * (cMax - cMin);
	lA = aMin;
	uA = aMax;
	if (cMin + b0 > lA)
		lA = cMin + b0;
	if (cMax + b0 < uA)
		uA = cMax + b0;

	luBounds[0] = lA;
	luBounds[1] = uA;

	return;
}

int SuperCluster::computeIndex(int m1, int m2, int m3, int axisdir) {
	int p = 0;
	if (m1 == axisdir)
		p = p + 1;
	if (m2 == axisdir)
		p = p + 2;
	if (m3 == axisdir)
		p = p + 4;

	return p;
}

double SuperCluster::analyticProduct(int p, double s01, double s02, double s03,
		double w1, double w2, double w3, std::vector<int> luBounds,
		int active) {
	double kOut = ap1(p, s01, s02, s03, w1, w2, w3, luBounds[0], luBounds[4]);
	kOut = kOut + ap2(p, s01, s02, s03, w1, w2, w3, luBounds[1], luBounds[5]);
	kOut = kOut + ap3(p, s01, s02, s03, w1, w2, w3, luBounds[2], luBounds[6]);
	kOut = kOut + ap4(p, s01, s02, s03, w1, w2, w3, luBounds[3], luBounds[7]);

	return kOut;
}

double SuperCluster::analyticReact(int p, double s01, double s02, double s03,
		double w1, double w2, double w3, std::vector<int> luBounds,
		int active) {
	double kOut = 0.0;
	if (active == 1) {
		kOut = a1r1(p, s01, s02, s03, w1, w2, w3, luBounds[0], luBounds[4]);
		kOut = kOut
				+ a1r2(p, s01, s02, s03, w1, w2, w3, luBounds[1], luBounds[5]);
		kOut = kOut
				+ a1r3(p, s01, s02, s03, w1, w2, w3, luBounds[2], luBounds[6]);
		kOut = kOut
				+ a1r4(p, s01, s02, s03, w1, w2, w3, luBounds[3], luBounds[7]);
	} else {
		kOut = a2r1(p, s01, s02, s03, w1, w2, w3, luBounds[0], luBounds[4]);
		kOut = kOut
				+ a2r2(p, s01, s02, s03, w1, w2, w3, luBounds[1], luBounds[5]);
		kOut = kOut
				+ a2r3(p, s01, s02, s03, w1, w2, w3, luBounds[2], luBounds[6]);
		kOut = kOut
				+ a2r4(p, s01, s02, s03, w1, w2, w3, luBounds[3], luBounds[7]);
	}

	return kOut;
}

double SuperCluster::ap1(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ap = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ap=-((-1.0+lb-ub)*(2.0+2.0*w2+2.0*w3+lb+ub+2.0*s02-2.0*s03))/2.0;
			break;
		case 1 : ap=-((-1.0+lb-ub)*(-6.0*s01-6.0*w2*s01-6.0*w3*s01+2.0*lb+3.0*w2*lb+3.0*w3*lb
				-3.0*s01*lb+2.0*lb*lb+4.0*ub+3.0*w2*ub+3.0*w3*ub-3.0*s01*ub+2.0*lb*ub
				+2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-3.0*ub*s03))/(6.*(1.0+w1));
			break;
		case 2 : ap=-((-1.0+lb-ub)*(3.0*w2+3.0*w2*w2-3.0*w3-3.0*w3*w3-lb-3.0*w3*lb-lb*lb
				-2.0*ub-3.0*w3*ub-lb*ub-ub*ub-3.0*s02-6.0*w3*s02-3.0*lb*s02-3.0*ub*s02
				-3.0*s02*s02+3.0*s03+6.0*w3*s03+3.0*lb*s03+3.0*ub*s03+6.0*s02*s03-3.0*s03*s03))/(6.*(1.0+w2));
			break;
		case 3 : ap=-((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01+2.0*lb
				+6.0*w2*lb+6.0*w2*w2*lb-2.0*w3*lb-6.0*w3*w3*lb+4.0*s01*lb+12.0*w3*s01*lb
				-lb*lb-8.0*w3*lb*lb+4.0*s01*lb*lb-3.0*lb*lb*lb-2.0*ub+6.0*w2*ub
				+6.0*w2*w2*ub-10.0*w3*ub-6.0*w3*w3*ub+8.0*s01*ub+12.0*w3*s01*ub
				-4.0*lb*ub-8.0*w3*lb*ub+4.0*s01*lb*ub-3.0*lb*lb*ub-7.0*ub*ub
				-8.0*w3*ub*ub+4.0*s01*ub*ub-3.0*lb*ub*ub-3.0*ub*ub*ub+12.0*s01*s02
				+24.0*w3*s01*s02-2.0*lb*s02-12.0*w3*lb*s02+12.0*s01*lb*s02-8.0*lb*lb*s02
				-10.0*ub*s02-12.0*w3*ub*s02+12.0*s01*ub*s02-8.0*lb*ub*s02-8.0*ub*ub*s02
				+12.0*s01*s02*s02-6.0*lb*s02*s02-6.0*ub*s02*s02-12.0*s01*s03-24.0*w3*s01*s03
				+2.0*lb*s03+12.0*w3*lb*s03-12.0*s01*lb*s03+8.0*lb*lb*s03+10.0*ub*s03
				+12.0*w3*ub*s03-12.0*s01*ub*s03+8.0*lb*ub*s03+8.0*ub*ub*s03-24.0*s01*s02*s03
				+12.0*lb*s02*s03+12.0*ub*s02*s03+12.0*s01*s03*s03-6.0*lb*s03*s03-6.0*ub*s03*s03))
						/(24.*(1.0+w1)*(1.0+w2));
			break;
		case 4 : ap=-((-1.0+lb-ub)*(3.0*w2+3.0*w2*w2-3.0*w3-3.0*w3*w3+lb+3.0*w2*lb+lb*lb
				+2.0*ub+3.0*w2*ub+lb*ub+ub*ub+3.0*s02+6.0*w2*s02+3.0*lb*s02+3.0*ub*s02
				+3.0*s02*s02-3.0*s03-6.0*w2*s03-3.0*lb*s03-3.0*ub*s03-6.0*s02*s03+3.0*s03*s03))/6.;
			break;
		case 5 : ap=-((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01-2.0*lb
				+2.0*w2*lb+6.0*w2*w2*lb-6.0*w3*lb-6.0*w3*w3*lb-4.0*s01*lb-12.0*w2*s01*lb
				+lb*lb+8.0*w2*lb*lb-4.0*s01*lb*lb+3.0*lb*lb*lb+2.0*ub+10.0*w2*ub
				+6.0*w2*w2*ub-6.0*w3*ub-6.0*w3*w3*ub-8.0*s01*ub-12.0*w2*s01*ub+4.0*lb*ub
				+8.0*w2*lb*ub-4.0*s01*lb*ub+3.0*lb*lb*ub+7.0*ub*ub+8.0*w2*ub*ub
				-4.0*s01*ub*ub+3.0*lb*ub*ub+3.0*ub*ub*ub-12.0*s01*s02-24.0*w2*s01*s02
				+2.0*lb*s02+12.0*w2*lb*s02-12.0*s01*lb*s02+8.0*lb*lb*s02+10.0*ub*s02
				+12.0*w2*ub*s02-12.0*s01*ub*s02+8.0*lb*ub*s02+8.0*ub*ub*s02-12.0*s01*s02*s02
				+6.0*lb*s02*s02+6.0*ub*s02*s02+12.0*s01*s03+24.0*w2*s01*s03-2.0*lb*s03
				-12.0*w2*lb*s03+12.0*s01*lb*s03-8.0*lb*lb*s03-10.0*ub*s03-12.0*w2*ub*s03
				+12.0*s01*ub*s03-8.0*lb*ub*s03-8.0*ub*ub*s03+24.0*s01*s02*s03-12.0*lb*s02*s03
				-12.0*ub*s02*s03-12.0*s01*s03*s03+6.0*lb*s03*s03+6.0*ub*s03*s03))/(24.*(1.0+w1));
			break;
		case 6 : ap=-((-1.0+lb-ub)*(4.0*w2+12.0*w2*w2+8.0*w2*w2*w2+4.0*w3+12.0*w3*w3+8.0*w3*w3*w3
				+2.0*lb+6.0*w2*lb+6.0*w2*w2*lb+6.0*w3*lb+6.0*w3*w3*lb+lb*lb-lb*lb*lb
				+2.0*ub+6.0*w2*ub+6.0*w2*w2*ub+6.0*w3*ub+6.0*w3*w3*ub-lb*lb*ub
				-ub*ub-lb*ub*ub-ub*ub*ub+4.0*s02+12.0*w2*s02+12.0*w2*w2*s02+12.0*w3*s02
				+12.0*w3*w3*s02+2.0*lb*s02-4.0*lb*lb*s02-2.0*ub*s02-4.0*lb*ub*s02
				-4.0*ub*ub*s02-6.0*lb*s02*s02-6.0*ub*s02*s02-4.0*s02*s02*s02-4.0*s03-12.0*w2*s03
				-12.0*w2*w2*s03-12.0*w3*s03-12.0*w3*w3*s03-2.0*lb*s03+4.0*lb*lb*s03
				+2.0*ub*s03+4.0*lb*ub*s03+4.0*ub*ub*s03+12.0*lb*s02*s03+12.0*ub*s02*s03
				+12.0*s02*s02*s03-6.0*lb*s03*s03-6.0*ub*s03*s03-12.0*s02*s03*s03+4.0*s03*s03*s03))/(24.*(1.0+w2));
			break;
		case 7 : ap=-((-1.0+lb-ub)*(-20.0*w2*s01-60.0*w2*w2*s01-40.0*w2*w2*w2*s01-20.0*w3*s01
				-60.0*w3*w3*s01-40.0*w3*w3*w3*s01-4.0*lb+20.0*w2*w2*lb+20.0*w2*w2*w2*lb
				+20.0*w3*w3*lb+20.0*w3*w3*w3*lb-10.0*s01*lb-30.0*w2*s01*lb-30.0*w2*w2*s01*lb
				-30.0*w3*s01*lb-30.0*w3*w3*s01*lb+6.0*lb*lb+20.0*w2*lb*lb
				+20.0*w2*w2*lb*lb+20.0*w3*lb*lb+20.0*w3*w3*lb*lb-5.0*s01*lb*lb
				+6.0*lb*lb*lb+5.0*s01*lb*lb*lb-4.0*lb*lb*lb*lb+4.0*ub+20.0*w2*ub+40.0*w2*w2*ub
				+20.0*w2*w2*w2*ub+20.0*w3*ub+40.0*w3*w3*ub+20.0*w3*w3*w3*ub-10.0*s01*ub
				-30.0*w2*s01*ub-30.0*w2*w2*s01*ub-30.0*w3*s01*ub-30.0*w3*w3*s01*ub
				+8.0*lb*ub+20.0*w2*lb*ub+20.0*w2*w2*lb*ub+20.0*w3*lb*ub
				+20.0*w3*w3*lb*ub+2.0*lb*lb*ub+5.0*s01*lb*lb*ub-4.0*lb*lb*lb*ub+6.0*ub*ub
				+20.0*w2*ub*ub+20.0*w2*w2*ub*ub+20.0*w3*ub*ub+20.0*w3*w3*ub*ub
				+5.0*s01*ub*ub-2.0*lb*ub*ub+5.0*s01*lb*ub*ub-4.0*lb*lb*ub*ub-6.0*ub*ub*ub
				+5.0*s01*ub*ub*ub-4.0*lb*ub*ub*ub-4.0*ub*ub*ub*ub-20.0*s01*s02-60.0*w2*s01*s02
				-60.0*w2*w2*s01*s02-60.0*w3*s01*s02-60.0*w3*w3*s01*s02+10.0*lb*s02
				+30.0*w2*lb*s02+30.0*w2*w2*lb*s02+30.0*w3*lb*s02+30.0*w3*w3*lb*s02
				-10.0*s01*lb*s02+15.0*lb*lb*s02+20.0*s01*lb*lb*s02-15.0*lb*lb*lb*s02+10.0*ub*s02
				+30.0*w2*ub*s02+30.0*w2*w2*ub*s02+30.0*w3*ub*s02+30.0*w3*w3*ub*s02
				+10.0*s01*ub*s02+20.0*s01*lb*ub*s02-15.0*lb*lb*ub*s02-15.0*ub*ub*s02
				+20.0*s01*ub*ub*s02-15.0*lb*ub*ub*s02-15.0*ub*ub*ub*s02+10.0*lb*s02*s02
				+30.0*s01*lb*s02*s02-20.0*lb*lb*s02*s02-10.0*ub*s02*s02+30.0*s01*ub*s02*s02
				-20.0*lb*ub*s02*s02-20.0*ub*ub*s02*s02+20.0*s01*s02*s02*s02-10.0*lb*s02*s02*s02
				-10.0*ub*s02*s02*s02+20.0*s01*s03+60.0*w2*s01*s03+60.0*w2*w2*s01*s03+60.0*w3*s01*s03
				+60.0*w3*w3*s01*s03-10.0*lb*s03-30.0*w2*lb*s03-30.0*w2*w2*lb*s03
				-30.0*w3*lb*s03-30.0*w3*w3*lb*s03+10.0*s01*lb*s03-15.0*lb*lb*s03
				-20.0*s01*lb*lb*s03+15.0*lb*lb*lb*s03-10.0*ub*s03-30.0*w2*ub*s03
				-30.0*w2*w2*ub*s03-30.0*w3*ub*s03-30.0*w3*w3*ub*s03-10.0*s01*ub*s03
				-20.0*s01*lb*ub*s03+15.0*lb*lb*ub*s03+15.0*ub*ub*s03-20.0*s01*ub*ub*s03
				+15.0*lb*ub*ub*s03+15.0*ub*ub*ub*s03-20.0*lb*s02*s03-60.0*s01*lb*s02*s03
				+40.0*lb*lb*s02*s03+20.0*ub*s02*s03-60.0*s01*ub*s02*s03+40.0*lb*ub*s02*s03
				+40.0*ub*ub*s02*s03-60.0*s01*s02*s02*s03+30.0*lb*s02*s02*s03+30.0*ub*s02*s02*s03
				+10.0*lb*s03*s03+30.0*s01*lb*s03*s03-20.0*lb*lb*s03*s03-10.0*ub*s03*s03
				+30.0*s01*ub*s03*s03-20.0*lb*ub*s03*s03-20.0*ub*ub*s03*s03+60.0*s01*s02*s03*s03
				-30.0*lb*s02*s03*s03-30.0*ub*s02*s03*s03-20.0*s01*s03*s03*s03+10.0*lb*s03*s03*s03
				+10.0*ub*s03*s03*s03))/(120.*(1.0+w1)*(1.0+w2));
			break;
		default : ap = 0.0;
			break;
	}

	return ap;
}

double SuperCluster::ap2(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ap = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ap=(1.0+2.0*w2)*(1.0-lb+ub);
			break;
		case 1 : ap=-((1.0+2.0*w2)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/(2.0*(1.0+w1));
			break;
		case 2 : ap=0.0;
			break;
		case 3 : ap=0.0;
			break;
		case 4 : ap=-((1.0+2.0*w2)*(-1.0+lb-ub)*(lb+ub+2.0*s02-2.0*s03))/2.0;
			break;
		case 5 : ap=-((1.0+2.0*w2)*(-1.0+lb-ub)*(-lb-3.0*s01*lb+2.0*lb*lb+ub-3.0*s01*ub
				+2.0*lb*ub+2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03
				-3.0*ub*s03))/(6.*(1.0+w1));
			break;
		case 6 : ap=(w2*(1.0+2.0*w2)*(1.0-lb+ub))/3.;
			break;
		case 7 : ap=-(w2*(1.0+2.0*w2)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/(6.*(1.0+w1));
			break;
		default : ap = 0.0;
			break;
	}

	return ap;
}

double SuperCluster::ap3(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ap = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ap=(1.0+2.0*w3)*(1.0-lb+ub);
			break;
		case 1 : ap=-((1.0+2.0*w3)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/(2.*(1.0+w1));
			break;
		case 2 : ap=((1.0+2.0*w3)*(-1.0+lb-ub)*(lb+ub+2.0*s02-2.0*s03))/(2.*(1.0+w2));
			break;
		case 3 : ap=((1.0+2.0*w3)*(-1.0+lb-ub)*(-lb-3.0*s01*lb+2.0*lb*lb+ub-3.0*s01*ub
				+2.0*lb*ub+2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03
				-3.0*ub*s03))/(6.*(1.0+w1)*(1.0+w2));
			break;
		case 4 : ap=0.0;
			break;
		case 5 : ap=0.0;
			break;
		case 6 : ap=((1.0+2.0*w3)*(w3+w3*w3)*(1.0-lb+ub))/(3.*(1.0+w2));
			break;
		case 7 : ap=-(w3*(1.0+w3)*(1.0+2.0*w3)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/(6.*(1.0+w1)*(1.0+w2));
			break;
		default : ap = 0.0;
			break;
	}

	return ap;
}

double SuperCluster::ap4(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ap = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ap=((-1.0+lb-ub)*(-2.0-2.0*w2-2.0*w3+lb+ub+2.0*s02-2.0*s03))/2.;
			break;
		case 1 : ap=((-1.0+lb-ub)*(6.0*s01+6.0*w2*s01+6.0*w3*s01-4.0*lb-3.0*w2*lb-3.0*w3*lb-
				3.0*s01*lb+2.0*lb*lb-2.0*ub-3.0*w2*ub-3.0*w3*ub-3.0*s01*ub+2.0*lb*ub+
				2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-3.0*ub*s03))/(6.*(1.0+w1));
			break;
		case 2 : ap=((-1.0+lb-ub)*(3.0*w2+3.0*w2*w2-3.0*w3-3.0*w3*w3+2.0*lb+3.0*w3*lb-lb*lb+
				ub+3.0*w3*ub-lb*ub-ub*ub+3.0*s02+6.0*w3*s02-3.0*lb*s02-3.0*ub*s02-
				3.0*s02*s02-3.0*s03-6.0*w3*s03+3.0*lb*s03+3.0*ub*s03+6.0*s02*s03-3.0*s03*s03))/(6.*(1.0+w2));
			break;
		case 3 : ap=((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01-2.0*lb+
				6.0*w2*lb+6.0*w2*w2*lb-10.0*w3*lb-6.0*w3*w3*lb-8.0*s01*lb-12.0*w3*s01*lb+
				7.0*lb*lb+8.0*w3*lb*lb+4.0*s01*lb*lb-3.0*lb*lb*lb+2.0*ub+6.0*w2*ub+
				6.0*w2*w2*ub-2.0*w3*ub-6.0*w3*w3*ub-4.0*s01*ub-12.0*w3*s01*ub+4.0*lb*ub+
				8.0*w3*lb*ub+4.0*s01*lb*ub-3.0*lb*lb*ub+ub*ub+8.0*w3*ub*ub+
				4.0*s01*ub*ub-3.0*lb*ub*ub-3.0*ub*ub*ub-12.0*s01*s02-24.0*w3*s01*s02+
				10.0*lb*s02+12.0*w3*lb*s02+12.0*s01*lb*s02-8.0*lb*lb*s02+2.0*ub*s02+
				12.0*w3*ub*s02+12.0*s01*ub*s02-8.0*lb*ub*s02-8.0*ub*ub*s02+12.0*s01*s02*s02-
				6.0*lb*s02*s02-6.0*ub*s02*s02+12.0*s01*s03+24.0*w3*s01*s03-10.0*lb*s03-
				12.0*w3*lb*s03-12.0*s01*lb*s03+8.0*lb*lb*s03-2.0*ub*s03-12.0*w3*ub*s03-
				12.0*s01*ub*s03+8.0*lb*ub*s03+8.0*ub*ub*s03-24.0*s01*s02*s03+12.0*lb*s02*s03+
				12.0*ub*s02*s03+12.0*s01*s03*s03-6.0*lb*s03*s03-6.0*ub*s03*s03))/(24.*(1.0+w1)*(1.0+w2));
			break;
		case 4 : ap=((-1.0+lb-ub)*(3.0*w2+3.0*w2*w2-3.0*w3-3.0*w3*w3-2.0*lb-3.0*w2*lb+lb*lb-
				ub-3.0*w2*ub+lb*ub+ub*ub-3.0*s02-6.0*w2*s02+3.0*lb*s02+3.0*ub*s02+
				3.0*s02*s02+3.0*s03+6.0*w2*s03-3.0*lb*s03-3.0*ub*s03-6.0*s02*s03+3.0*s03*s03))/6.;
			break;
		case 5 : ap=((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01+2.0*lb+
				10.0*w2*lb+6.0*w2*w2*lb-6.0*w3*lb-6.0*w3*w3*lb+8.0*s01*lb+12.0*w2*s01*lb-
				7.0*lb*lb-8.0*w2*lb*lb-4.0*s01*lb*lb+3.0*lb*lb*lb-2.0*ub+2.0*w2*ub+
				6.0*w2*w2*ub-6.0*w3*ub-6.0*w3*w3*ub+4.0*s01*ub+12.0*w2*s01*ub-4.0*lb*ub-
				8.0*w2*lb*ub-4.0*s01*lb*ub+3.0*lb*lb*ub-ub*ub-8.0*w2*ub*ub-
				4.0*s01*ub*ub+3.0*lb*ub*ub+3.0*ub*ub*ub+12.0*s01*s02+24.0*w2*s01*s02-
				10.0*lb*s02-12.0*w2*lb*s02-12.0*s01*lb*s02+8.0*lb*lb*s02-2.0*ub*s02-
				12.0*w2*ub*s02-12.0*s01*ub*s02+8.0*lb*ub*s02+8.0*ub*ub*s02-12.0*s01*s02*s02+
				6.0*lb*s02*s02+6.0*ub*s02*s02-12.0*s01*s03-24.0*w2*s01*s03+10.0*lb*s03+
				12.0*w2*lb*s03+12.0*s01*lb*s03-8.0*lb*lb*s03+2.0*ub*s03+12.0*w2*ub*s03+
				12.0*s01*ub*s03-8.0*lb*ub*s03-8.0*ub*ub*s03+24.0*s01*s02*s03-12.0*lb*s02*s03-
				12.0*ub*s02*s03-12.0*s01*s03*s03+6.0*lb*s03*s03+6.0*ub*s03*s03))/(24.*(1.0+w1));
			break;
		case 6 : ap=-((-1.0+lb-ub)*(4.0*w2+12.0*w2*w2+8.0*w2*w2*w2+4.0*w3+12.0*w3*w3+8.0*w3*w3*w3-
				2.0*lb-6.0*w2*lb-6.0*w2*w2*lb-6.0*w3*lb-6.0*w3*w3*lb-lb*lb+lb*lb*lb-
				2.0*ub-6.0*w2*ub-6.0*w2*w2*ub-6.0*w3*ub-6.0*w3*w3*ub+lb*lb*ub+
				ub*ub+lb*ub*ub+ub*ub*ub-4.0*s02-12.0*w2*s02-12.0*w2*w2*s02-12.0*w3*s02-
				12.0*w3*w3*s02-2.0*lb*s02+4.0*lb*lb*s02+2.0*ub*s02+4.0*lb*ub*s02+
				4.0*ub*ub*s02+6.0*lb*s02*s02+6.0*ub*s02*s02+4.0*s02*s02*s02+4.0*s03+12.0*w2*s03+
				12.0*w2*w2*s03+12.0*w3*s03+12.0*w3*w3*s03+2.0*lb*s03-4.0*lb*lb*s03-
				2.0*ub*s03-4.0*lb*ub*s03-4.0*ub*ub*s03-12.0*lb*s02*s03-12.0*ub*s02*s03-
				12.0*s02*s02*s03+6.0*lb*s03*s03+6.0*ub*s03*s03+12.0*s02*s03*s03-4.0*s03*s03*s03))/(24.*(1.0+w2));
			break;
		case 7 : ap=-((-1.0+lb-ub)*(-20.0*w2*s01-60.0*w2*w2*s01-40.0*w2*w2*w2*s01-20.0*w3*s01-
				60.0*w3*w3*s01-40.0*w3*w3*w3*s01+4.0*lb+20.0*w2*lb+40.0*w2*w2*lb+
				20.0*w2*w2*w2*lb+20.0*w3*lb+40.0*w3*w3*lb+20.0*w3*w3*w3*lb+10.0*s01*lb+
				30.0*w2*s01*lb+30.0*w2*w2*s01*lb+30.0*w3*s01*lb+30.0*w3*w3*s01*lb-
				6.0*lb*lb-20.0*w2*lb*lb-20.0*w2*w2*lb*lb-20.0*w3*lb*lb-
				20.0*w3*w3*lb*lb+5.0*s01*lb*lb-6.0*lb*lb*lb-5.0*s01*lb*lb*lb+4.0*lb*lb*lb*lb-4.0*ub+
				20.0*w2*w2*ub+20.0*w2*w2*w2*ub+20.0*w3*w3*ub+20.0*w3*w3*w3*ub+10.0*s01*ub+
				30.0*w2*s01*ub+30.0*w2*w2*s01*ub+30.0*w3*s01*ub+30.0*w3*w3*s01*ub-
				8.0*lb*ub-20.0*w2*lb*ub-20.0*w2*w2*lb*ub-20.0*w3*lb*ub-
				20.0*w3*w3*lb*ub-2.0*lb*lb*ub-5.0*s01*lb*lb*ub+4.0*lb*lb*lb*ub-6.0*ub*ub-
				20.0*w2*ub*ub-20.0*w2*w2*ub*ub-20.0*w3*ub*ub-20.0*w3*w3*ub*ub-
				5.0*s01*ub*ub+2.0*lb*ub*ub-5.0*s01*lb*ub*ub+4.0*lb*lb*ub*ub+6.0*ub*ub*ub-
				5.0*s01*ub*ub*ub+4.0*lb*ub*ub*ub+4.0*ub*ub*ub*ub+20.0*s01*s02+60.0*w2*s01*s02+
				60.0*w2*w2*s01*s02+60.0*w3*s01*s02+60.0*w3*w3*s01*s02-10.0*lb*s02-
				30.0*w2*lb*s02-30.0*w2*w2*lb*s02-30.0*w3*lb*s02-30.0*w3*w3*lb*s02+
				10.0*s01*lb*s02-15.0*lb*lb*s02-20.0*s01*lb*lb*s02+15.0*lb*lb*lb*s02-10.0*ub*s02-
				30.0*w2*ub*s02-30.0*w2*w2*ub*s02-30.0*w3*ub*s02-30.0*w3*w3*ub*s02-
				10.0*s01*ub*s02-20.0*s01*lb*ub*s02+15.0*lb*lb*ub*s02+15.0*ub*ub*s02-
				20.0*s01*ub*ub*s02+15.0*lb*ub*ub*s02+15.0*ub*ub*ub*s02-10.0*lb*s02*s02-
				30.0*s01*lb*s02*s02+20.0*lb*lb*s02*s02+10.0*ub*s02*s02-30.0*s01*ub*s02*s02+
				20.0*lb*ub*s02*s02+20.0*ub*ub*s02*s02-20.0*s01*s02*s02*s02+10.0*lb*s02*s02*s02+
				10.0*ub*s02*s02*s02-20.0*s01*s03-60.0*w2*s01*s03-60.0*w2*w2*s01*s03-60.0*w3*s01*s03-
				60.0*w3*w3*s01*s03+10.0*lb*s03+30.0*w2*lb*s03+30.0*w2*w2*lb*s03+
				30.0*w3*lb*s03+30.0*w3*w3*lb*s03-10.0*s01*lb*s03+15.0*lb*lb*s03+
				20.0*s01*lb*lb*s03-15.0*lb*lb*lb*s03+10.0*ub*s03+30.0*w2*ub*s03+
				30.0*w2*w2*ub*s03+30.0*w3*ub*s03+30.0*w3*w3*ub*s03+10.0*s01*ub*s03+
				20.0*s01*lb*ub*s03-15.0*lb*lb*ub*s03-15.0*ub*ub*s03+20.0*s01*ub*ub*s03-
				15.0*lb*ub*ub*s03-15.0*ub*ub*ub*s03+20.0*lb*s02*s03+60.0*s01*lb*s02*s03-
				40.0*lb*lb*s02*s03-20.0*ub*s02*s03+60.0*s01*ub*s02*s03-40.0*lb*ub*s02*s03-
				40.0*ub*ub*s02*s03+60.0*s01*s02*s02*s03-30.0*lb*s02*s02*s03-30.0*ub*s02*s02*s03-
				10.0*lb*s03*s03-30.0*s01*lb*s03*s03+20.0*lb*lb*s03*s03+10.0*ub*s03*s03-
				30.0*s01*ub*s03*s03+20.0*lb*ub*s03*s03+20.0*ub*ub*s03*s03-60.0*s01*s02*s03*s03+
				30.0*lb*s02*s03*s03+30.0*ub*s02*s03*s03+20.0*s01*s03*s03*s03-10.0*lb*s03*s03*s03-
				10.0*ub*s03*s03*s03))/(120.*(1.0+w1)*(1.0+w2));
			break;
		default : ap = 0.0;
			break;
	}

	return ap;
}

double SuperCluster::a1r1(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ar=-((-1.0+lb-ub)*(2.0+2.0*w2+2.0*w3+lb+ub+2.0*s02-2.0*s03))/2.;
			break;
		case 1 : ar=-((-1.0+lb-ub)*(-6.0*s01-6.0*w2*s01-6.0*w3*s01+2.0*lb+3.0*w2*lb+3.0*w3*lb-
				3.0*s01*lb+2.0*lb*lb+4.0*ub+3.0*w2*ub+3.0*w3*ub-3.0*s01*ub+2.0*lb*ub+
				2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-3.0*ub*s03))/(6.*(1.0+w1));
			break;
		case 2 : ar=-((-1.0+lb-ub)*(3.0*w2+3.0*w2*w2-3.0*w3-3.0*w3*w3-lb-3.0*w3*lb-lb*lb-
				2.0*ub-3.0*w3*ub-lb*ub-ub*ub-3.0*s02-6.0*w3*s02-3.0*lb*s02-3.0*ub*s02-
				3.0*s02*s02+3.0*s03+6.0*w3*s03+3.0*lb*s03+3.0*ub*s03+6.0*s02*s03-3.0*s03*s03))/(6.*(1.0+w2));
			break;
		case 3 : ar=-((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01+2.0*lb+
				6.0*w2*lb+6.0*w2*w2*lb-2.0*w3*lb-6.0*w3*w3*lb+4.0*s01*lb+12.0*w3*s01*lb-
				lb*lb-8.0*w3*lb*lb+4.0*s01*lb*lb-3.0*lb*lb*lb-2.0*ub+6.0*w2*ub+
				6.0*w2*w2*ub-10.0*w3*ub-6.0*w3*w3*ub+8.0*s01*ub+12.0*w3*s01*ub-
				4.0*lb*ub-8.0*w3*lb*ub+4.0*s01*lb*ub-3.0*lb*lb*ub-7.0*ub*ub-
				8.0*w3*ub*ub+4.0*s01*ub*ub-3.0*lb*ub*ub-3.0*ub*ub*ub+12.0*s01*s02+
				24.0*w3*s01*s02-2.0*lb*s02-12.0*w3*lb*s02+12.0*s01*lb*s02-8.0*lb*lb*s02-
				10.0*ub*s02-12.0*w3*ub*s02+12.0*s01*ub*s02-8.0*lb*ub*s02-8.0*ub*ub*s02+
				12.0*s01*s02*s02-6.0*lb*s02*s02-6.0*ub*s02*s02-12.0*s01*s03-24.0*w3*s01*s03+
				2.0*lb*s03+12.0*w3*lb*s03-12.0*s01*lb*s03+8.0*lb*lb*s03+10.0*ub*s03+
				12.0*w3*ub*s03-12.0*s01*ub*s03+8.0*lb*ub*s03+8.0*ub*ub*s03-24.0*s01*s02*s03+
				12.0*lb*s02*s03+12.0*ub*s02*s03+12.0*s01*s03*s03-6.0*lb*s03*s03-6.0*ub*s03*s03))/
						(24.*(1.0+w1)*(1.0+w2)); break;
		case 4 : ar=-((-1.0+lb-ub)*(-6.0*s01-6.0*w2*s01-6.0*w3*s01+2.0*lb+3.0*w2*lb+3.0*w3*lb-
				3.0*s01*lb+2.0*lb*lb+4.0*ub+3.0*w2*ub+3.0*w3*ub-3.0*s01*ub+2.0*lb*ub+
				2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-3.0*ub*s03))/6.;
			break;
		case 5 : ar=-((-1.0+lb-ub)*(12.0*s01*s01+12.0*w2*s01*s01+12.0*w3*s01*s01-2.0*lb-2.0*w2*lb-
				2.0*w3*lb-8.0*s01*lb-12.0*w2*s01*lb-12.0*w3*s01*lb+6.0*s01*s01*lb+lb*lb+
				4.0*w2*lb*lb+4.0*w3*lb*lb-8.0*s01*lb*lb+3.0*lb*lb*lb+2.0*ub+2.0*w2*ub+
				2.0*w3*ub-16.0*s01*ub-12.0*w2*s01*ub-12.0*w3*s01*ub+6.0*s01*s01*ub+
				4.0*lb*ub+4.0*w2*lb*ub+4.0*w3*lb*ub-8.0*s01*lb*ub+3.0*lb*lb*ub+
				7.0*ub*ub+4.0*w2*ub*ub+4.0*w3*ub*ub-8.0*s01*ub*ub+3.0*lb*ub*ub+
				3.0*ub*ub*ub+12.0*s01*s01*s02-2.0*lb*s02-12.0*s01*lb*s02+4.0*lb*lb*s02+
				2.0*ub*s02-12.0*s01*ub*s02+4.0*lb*ub*s02+4.0*ub*ub*s02-12.0*s01*s01*s03+
				2.0*lb*s03+12.0*s01*lb*s03-4.0*lb*lb*s03-2.0*ub*s03+12.0*s01*ub*s03-
				4.0*lb*ub*s03-4.0*ub*ub*s03))/(12.*(1.0+w1));
			break;
		case 6 : ar=-((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01+2.0*lb+
				6.0*w2*lb+6.0*w2*w2*lb-2.0*w3*lb-6.0*w3*w3*lb+4.0*s01*lb+12.0*w3*s01*lb-
				lb*lb-8.0*w3*lb*lb+4.0*s01*lb*lb-3.0*lb*lb*lb-2.0*ub+6.0*w2*ub+
				6.0*w2*w2*ub-10.0*w3*ub-6.0*w3*w3*ub+8.0*s01*ub+12.0*w3*s01*ub-
				4.0*lb*ub-8.0*w3*lb*ub+4.0*s01*lb*ub-3.0*lb*lb*ub-7.0*ub*ub-
				8.0*w3*ub*ub+4.0*s01*ub*ub-3.0*lb*ub*ub-3.0*ub*ub*ub+12.0*s01*s02+
				24.0*w3*s01*s02-2.0*lb*s02-12.0*w3*lb*s02+12.0*s01*lb*s02-8.0*lb*lb*s02-
				10.0*ub*s02-12.0*w3*ub*s02+12.0*s01*ub*s02-8.0*lb*ub*s02-8.0*ub*ub*s02+
				12.0*s01*s02*s02-6.0*lb*s02*s02-6.0*ub*s02*s02-12.0*s01*s03-24.0*w3*s01*s03+
				2.0*lb*s03+12.0*w3*lb*s03-12.0*s01*lb*s03+8.0*lb*lb*s03+10.0*ub*s03+
				12.0*w3*ub*s03-12.0*s01*ub*s03+8.0*lb*ub*s03+8.0*ub*ub*s03-24.0*s01*s02*s03+
				12.0*lb*s02*s03+12.0*ub*s02*s03+12.0*s01*s03*s03-6.0*lb*s03*s03-6.0*ub*s03*s03))/(24.*(1.0+w2));
			break;
		case 7 : ar=-((-1.0+lb-ub)*(60.0*w2*s01*s01+60.0*w2*w2*s01*s01-60.0*w3*s01*s01-
				60.0*w3*w3*s01*s01-2.0*lb-10.0*w2*lb-10.0*w2*w2*lb+10.0*w3*lb+
				10.0*w3*w3*lb-20.0*s01*lb-60.0*w2*s01*lb-60.0*w2*w2*s01*lb+20.0*w3*s01*lb+
				60.0*w3*w3*s01*lb-20.0*s01*s01*lb-60.0*w3*s01*s01*lb+13.0*lb*lb+
				20.0*w2*lb*lb+20.0*w2*w2*lb*lb+10.0*w3*lb*lb-20.0*w3*w3*lb*lb+
				10.0*s01*lb*lb+80.0*w3*s01*lb*lb-20.0*s01*s01*lb*lb+3.0*lb*lb*lb-
				30.0*w3*lb*lb*lb+30.0*s01*lb*lb*lb-12.0*lb*lb*lb*lb+2.0*ub+10.0*w2*ub+
				10.0*w2*w2*ub-10.0*w3*ub-10.0*w3*w3*ub+20.0*s01*ub-60.0*w2*s01*ub-
				60.0*w2*w2*s01*ub+100.0*w3*s01*ub+60.0*w3*w3*s01*ub-40.0*s01*s01*ub-
				60.0*w3*s01*s01*ub+4.0*lb*ub+20.0*w2*lb*ub+20.0*w2*w2*lb*ub-
				20.0*w3*lb*ub-20.0*w3*w3*lb*ub+40.0*s01*lb*ub+80.0*w3*s01*lb*ub-
				20.0*s01*s01*lb*ub-9.0*lb*lb*ub-30.0*w3*lb*lb*ub+30.0*s01*lb*lb*ub-
				12.0*lb*lb*lb*ub-17.0*ub*ub+20.0*w2*ub*ub+20.0*w2*w2*ub*ub-50.0*w3*ub*ub-
				20.0*w3*w3*ub*ub+70.0*s01*ub*ub+80.0*w3*s01*ub*ub-20.0*s01*s01*ub*ub-
				21.0*lb*ub*ub-30.0*w3*lb*ub*ub+30.0*s01*lb*ub*ub-12.0*lb*lb*ub*ub-
				33.0*ub*ub*ub-30.0*w3*ub*ub*ub+30.0*s01*ub*ub*ub-12.0*lb*ub*ub*ub-12.0*ub*ub*ub*ub-
				60.0*s01*s01*s02-120.0*w3*s01*s01*s02+10.0*lb*s02+20.0*w3*lb*s02+
				20.0*s01*lb*s02+120.0*w3*s01*lb*s02-60.0*s01*s01*lb*s02+10.0*lb*lb*s02-
				40.0*w3*lb*lb*s02+80.0*s01*lb*lb*s02-30.0*lb*lb*lb*s02-10.0*ub*s02-
				20.0*w3*ub*s02+100.0*s01*ub*s02+120.0*w3*s01*ub*s02-60.0*s01*s01*ub*s02-
				20.0*lb*ub*s02-40.0*w3*lb*ub*s02+80.0*s01*lb*ub*s02-30.0*lb*lb*ub*s02-
				50.0*ub*ub*s02-40.0*w3*ub*ub*s02+80.0*s01*ub*ub*s02-30.0*lb*ub*ub*s02-
				30.0*ub*ub*ub*s02-60.0*s01*s01*s02*s02+10.0*lb*s02*s02+60.0*s01*lb*s02*s02-
				20.0*lb*lb*s02*s02-10.0*ub*s02*s02+60.0*s01*ub*s02*s02-20.0*lb*ub*s02*s02-
				20.0*ub*ub*s02*s02+60.0*s01*s01*s03+120.0*w3*s01*s01*s03-10.0*lb*s03-
				20.0*w3*lb*s03-20.0*s01*lb*s03-120.0*w3*s01*lb*s03+60.0*s01*s01*lb*s03-
				10.0*lb*lb*s03+40.0*w3*lb*lb*s03-80.0*s01*lb*lb*s03+30.0*lb*lb*lb*s03+
				10.0*ub*s03+20.0*w3*ub*s03-100.0*s01*ub*s03-120.0*w3*s01*ub*s03+
				60.0*s01*s01*ub*s03+20.0*lb*ub*s03+40.0*w3*lb*ub*s03-80.0*s01*lb*ub*s03+
				30.0*lb*lb*ub*s03+50.0*ub*ub*s03+40.0*w3*ub*ub*s03-80.0*s01*ub*ub*s03+
				30.0*lb*ub*ub*s03+30.0*ub*ub*ub*s03+120.0*s01*s01*s02*s03-20.0*lb*s02*s03-
				120.0*s01*lb*s02*s03+40.0*lb*lb*s02*s03+20.0*ub*s02*s03-120.0*s01*ub*s02*s03+
				40.0*lb*ub*s02*s03+40.0*ub*ub*s02*s03-60.0*s01*s01*s03*s03+10.0*lb*s03*s03+
				60.0*s01*lb*s03*s03-20.0*lb*lb*s03*s03-10.0*ub*s03*s03+60.0*s01*ub*s03*s03-
				20.0*lb*ub*s03*s03-20.0*ub*ub*s03*s03))/(120.*(1.0+w1)*(1.0+w2));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::a1r2(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ar=(1.0+2.0*w2)*(1.0-lb+ub);
			break;
		case 1 : ar=-((1.0+2.0*w2)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/(2.*(1.0+w1));
			break;
		case 2 : ar=0.0;
			break;
		case 3 : ar=0.0;
			break;
		case 4 : ar=-((1.0+2.0*w2)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/2.;
			break;
		case 5 : ar=-((1.0+2.0*w2)*(-1.0+lb-ub)*(6.0*s01*s01-lb-6.0*s01*lb+2.0*lb*lb+ub-
				6.0*s01*ub+2.0*lb*ub+2.0*ub*ub))/(6.*(1.0+w1));
			break;
		case 6 : ar=0.0;
			break;
		case 7 : ar=0.0;
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::a1r3(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ar=(1.0+2.0*w3)*(1.0-lb+ub);
			break;
		case 1 : ar=-((1.0+2.0*w3)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/(2.*(1.0+w1));
			break;
		case 2 : ar=((1.0+2.0*w3)*(-1.0+lb-ub)*(lb+ub+2.0*s02-2.0*s03))/(2.*(1.0+w2));
			break;
		case 3 : ar=((1.0+2.0*w3)*(-1.0+lb-ub)*(-lb-3.0*s01*lb+2.0*lb*lb+ub-3.0*s01*ub+
				2.0*lb*ub+2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-
				3.0*ub*s03))/(6.*(1.0+w1)*(1.0+w2));
			break;
		case 4 : ar=-((1.0+2.0*w3)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/2.;
			break;
		case 5 : ar=-((1.0+2.0*w3)*(-1.0+lb-ub)*(6.0*s01*s01-lb-6.0*s01*lb+2.0*lb*lb+ub-
				6.0*s01*ub+2.0*lb*ub+2.0*ub*ub))/(6.*(1.0+w1));
			break;
		case 6 : ar=((1.0+2.0*w3)*(-1.0+lb-ub)*(-lb-3.0*s01*lb+2.0*lb*lb+ub-3.0*s01*ub+
				2.0*lb*ub+2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-
				3.0*ub*s03))/(6.*(1.0+w2));
			break;
		case 7 : ar=((1.0+2.0*w3)*(-1.0+lb-ub)*(4.0*s01*lb+6.0*s01*s01*lb-3.0*lb*lb-8.0*s01*lb*lb+
				3.0*lb*lb*lb-4.0*s01*ub+6.0*s01*s01*ub-8.0*s01*lb*ub+3.0*lb*lb*ub+3.0*ub*ub-
				8.0*s01*ub*ub+3.0*lb*ub*ub+3.0*ub*ub*ub+12.0*s01*s01*s02-2.0*lb*s02-
				12.0*s01*lb*s02+4.0*lb*lb*s02+2.0*ub*s02-12.0*s01*ub*s02+4.0*lb*ub*s02+
				4.0*ub*ub*s02-12.0*s01*s01*s03+2.0*lb*s03+12.0*s01*lb*s03-4.0*lb*lb*s03-
				2.0*ub*s03+12.0*s01*ub*s03-4.0*lb*ub*s03-4.0*ub*ub*s03))/(12.*(1.0+w1)*(1.0+w2));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::a1r4(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ar=((-1.0+lb-ub)*(-2.0-2.0*w2-2.0*w3+lb+ub+2.0*s02-2.0*s03))/2.;
			break;
		case 1 : ar=((-1.0+lb-ub)*(6.0*s01+6.0*w2*s01+6.0*w3*s01-4.0*lb-3.0*w2*lb-3.0*w3*lb-
				3.0*s01*lb+2.0*lb*lb-2.0*ub-3.0*w2*ub-3.0*w3*ub-3.0*s01*ub+2.0*lb*ub+
				2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-3.0*ub*s03))/(6.*(1.0+w1));
			break;
		case 2 : ar=((-1.0+lb-ub)*(3.0*w2+3.0*w2*w2-3.0*w3-3.0*w3*w3+2.0*lb+3.0*w3*lb-lb*lb+
				ub+3.0*w3*ub-lb*ub-ub*ub+3.0*s02+6.0*w3*s02-3.0*lb*s02-3.0*ub*s02-
				3.0*s02*s02-3.0*s03-6.0*w3*s03+3.0*lb*s03+3.0*ub*s03+6.0*s02*s03-3.0*s03*s03))/(6.*(1.0+w2));
			break;
		case 3 : ar=((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01-2.0*lb+
				6.0*w2*lb+6.0*w2*w2*lb-10.0*w3*lb-6.0*w3*w3*lb-8.0*s01*lb-12.0*w3*s01*lb+
				7.0*lb*lb+8.0*w3*lb*lb+4.0*s01*lb*lb-3.0*lb*lb*lb+2.0*ub+6.0*w2*ub+
				6.0*w2*w2*ub-2.0*w3*ub-6.0*w3*w3*ub-4.0*s01*ub-12.0*w3*s01*ub+4.0*lb*ub+
				8.0*w3*lb*ub+4.0*s01*lb*ub-3.0*lb*lb*ub+ub*ub+8.0*w3*ub*ub+
				4.0*s01*ub*ub-3.0*lb*ub*ub-3.0*ub*ub*ub-12.0*s01*s02-24.0*w3*s01*s02+
				10.0*lb*s02+12.0*w3*lb*s02+12.0*s01*lb*s02-8.0*lb*lb*s02+2.0*ub*s02+
				12.0*w3*ub*s02+12.0*s01*ub*s02-8.0*lb*ub*s02-8.0*ub*ub*s02+12.0*s01*s02*s02-
				6.0*lb*s02*s02-6.0*ub*s02*s02+12.0*s01*s03+24.0*w3*s01*s03-10.0*lb*s03-
				12.0*w3*lb*s03-12.0*s01*lb*s03+8.0*lb*lb*s03-2.0*ub*s03-12.0*w3*ub*s03-
				12.0*s01*ub*s03+8.0*lb*ub*s03+8.0*ub*ub*s03-24.0*s01*s02*s03+12.0*lb*s02*s03+
				12.0*ub*s02*s03+12.0*s01*s03*s03-6.0*lb*s03*s03-6.0*ub*s03*s03))/(24.*(1.0+w1)*(1.0+w2));
			break;
		case 4 : ar=((-1.0+lb-ub)*(6.0*s01+6.0*w2*s01+6.0*w3*s01-4.0*lb-3.0*w2*lb-3.0*w3*lb-
				3.0*s01*lb+2.0*lb*lb-2.0*ub-3.0*w2*ub-3.0*w3*ub-3.0*s01*ub+2.0*lb*ub+
				2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-3.0*ub*s03))/6.;
			break;
		case 5 : ar=((-1.0+lb-ub)*(-12.0*s01*s01-12.0*w2*s01*s01-12.0*w3*s01*s01+2.0*lb+2.0*w2*lb+
				2.0*w3*lb+16.0*s01*lb+12.0*w2*s01*lb+12.0*w3*s01*lb+6.0*s01*s01*lb-
				7.0*lb*lb-4.0*w2*lb*lb-4.0*w3*lb*lb-8.0*s01*lb*lb+3.0*lb*lb*lb-2.0*ub-
				2.0*w2*ub-2.0*w3*ub+8.0*s01*ub+12.0*w2*s01*ub+12.0*w3*s01*ub+6.0*s01*s01*ub-
				4.0*lb*ub-4.0*w2*lb*ub-4.0*w3*lb*ub-8.0*s01*lb*ub+3.0*lb*lb*ub-ub*ub-
				4.0*w2*ub*ub-4.0*w3*ub*ub-8.0*s01*ub*ub+3.0*lb*ub*ub+3.0*ub*ub*ub+
				12.0*s01*s01*s02-2.0*lb*s02-12.0*s01*lb*s02+4.0*lb*lb*s02+2.0*ub*s02-
				12.0*s01*ub*s02+4.0*lb*ub*s02+4.0*ub*ub*s02-12.0*s01*s01*s03+2.0*lb*s03+
				12.0*s01*lb*s03-4.0*lb*lb*s03-2.0*ub*s03+12.0*s01*ub*s03-4.0*lb*ub*s03-
				4.0*ub*ub*s03))/(12.*(1.0+w1));
			break;
		case 6 : ar=((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01-2.0*lb+
				6.0*w2*lb+6.0*w2*w2*lb-10.0*w3*lb-6.0*w3*w3*lb-8.0*s01*lb-12.0*w3*s01*lb+
				7.0*lb*lb+8.0*w3*lb*lb+4.0*s01*lb*lb-3.0*lb*lb*lb+2.0*ub+6.0*w2*ub+
				6.0*w2*w2*ub-2.0*w3*ub-6.0*w3*w3*ub-4.0*s01*ub-12.0*w3*s01*ub+4.0*lb*ub+
				8.0*w3*lb*ub+4.0*s01*lb*ub-3.0*lb*lb*ub+ub*ub+8.0*w3*ub*ub+
				4.0*s01*ub*ub-3.0*lb*ub*ub-3.0*ub*ub*ub-12.0*s01*s02-24.0*w3*s01*s02+
				10.0*lb*s02+12.0*w3*lb*s02+12.0*s01*lb*s02-8.0*lb*lb*s02+2.0*ub*s02+
				12.0*w3*ub*s02+12.0*s01*ub*s02-8.0*lb*ub*s02-8.0*ub*ub*s02+12.0*s01*s02*s02-
				6.0*lb*s02*s02-6.0*ub*s02*s02+12.0*s01*s03+24.0*w3*s01*s03-10.0*lb*s03-
				12.0*w3*lb*s03-12.0*s01*lb*s03+8.0*lb*lb*s03-2.0*ub*s03-12.0*w3*ub*s03-
				12.0*s01*ub*s03+8.0*lb*ub*s03+8.0*ub*ub*s03-24.0*s01*s02*s03+12.0*lb*s02*s03+
				12.0*ub*s02*s03+12.0*s01*s03*s03-6.0*lb*s03*s03-6.0*ub*s03*s03))/(24.*(1.0+w2));
			break;
		case 7 : ar=((-1.0+lb-ub)*(60.0*w2*s01*s01+60.0*w2*w2*s01*s01-60.0*w3*s01*s01-
				60.0*w3*w3*s01*s01-2.0*lb-10.0*w2*lb-10.0*w2*w2*lb+10.0*w3*lb+
				10.0*w3*w3*lb+20.0*s01*lb-60.0*w2*s01*lb-60.0*w2*w2*s01*lb+100.0*w3*s01*lb+
				60.0*w3*w3*s01*lb+40.0*s01*s01*lb+60.0*w3*s01*s01*lb-17.0*lb*lb+
				20.0*w2*lb*lb+20.0*w2*w2*lb*lb-50.0*w3*lb*lb-20.0*w3*w3*lb*lb-
				70.0*s01*lb*lb-80.0*w3*s01*lb*lb-20.0*s01*s01*lb*lb+33.0*lb*lb*lb+
				30.0*w3*lb*lb*lb+30.0*s01*lb*lb*lb-12.0*lb*lb*lb*lb+2.0*ub+10.0*w2*ub+10.0*w2*w2*ub-
				10.0*w3*ub-10.0*w3*w3*ub-20.0*s01*ub-60.0*w2*s01*ub-60.0*w2*w2*s01*ub+
				20.0*w3*s01*ub+60.0*w3*w3*s01*ub+20.0*s01*s01*ub+60.0*w3*s01*s01*ub+
				4.0*lb*ub+20.0*w2*lb*ub+20.0*w2*w2*lb*ub-20.0*w3*lb*ub-
				20.0*w3*w3*lb*ub-40.0*s01*lb*ub-80.0*w3*s01*lb*ub-20.0*s01*s01*lb*ub+
				21.0*lb*lb*ub+30.0*w3*lb*lb*ub+30.0*s01*lb*lb*ub-12.0*lb*lb*lb*ub+
				13.0*ub*ub+20.0*w2*ub*ub+20.0*w2*w2*ub*ub+10.0*w3*ub*ub-
				20.0*w3*w3*ub*ub-10.0*s01*ub*ub-80.0*w3*s01*ub*ub-20.0*s01*s01*ub*ub+
				9.0*lb*ub*ub+30.0*w3*lb*ub*ub+30.0*s01*lb*ub*ub-12.0*lb*lb*ub*ub-
				3.0*ub*ub*ub+30.0*w3*ub*ub*ub+30.0*s01*ub*ub*ub-12.0*lb*ub*ub*ub-12.0*ub*ub*ub*ub+
				60.0*s01*s01*s02+120.0*w3*s01*s01*s02-10.0*lb*s02-20.0*w3*lb*s02-
				100.0*s01*lb*s02-120.0*w3*s01*lb*s02-60.0*s01*s01*lb*s02+50.0*lb*lb*s02+
				40.0*w3*lb*lb*s02+80.0*s01*lb*lb*s02-30.0*lb*lb*lb*s02+10.0*ub*s02+
				20.0*w3*ub*s02-20.0*s01*ub*s02-120.0*w3*s01*ub*s02-60.0*s01*s01*ub*s02+
				20.0*lb*ub*s02+40.0*w3*lb*ub*s02+80.0*s01*lb*ub*s02-30.0*lb*lb*ub*s02-
				10.0*ub*ub*s02+40.0*w3*ub*ub*s02+80.0*s01*ub*ub*s02-30.0*lb*ub*ub*s02-
				30.0*ub*ub*ub*s02-60.0*s01*s01*s02*s02+10.0*lb*s02*s02+60.0*s01*lb*s02*s02-
				20.0*lb*lb*s02*s02-10.0*ub*s02*s02+60.0*s01*ub*s02*s02-20.0*lb*ub*s02*s02-
				20.0*ub*ub*s02*s02-60.0*s01*s01*s03-120.0*w3*s01*s01*s03+10.0*lb*s03+
				20.0*w3*lb*s03+100.0*s01*lb*s03+120.0*w3*s01*lb*s03+60.0*s01*s01*lb*s03-
				50.0*lb*lb*s03-40.0*w3*lb*lb*s03-80.0*s01*lb*lb*s03+30.0*lb*lb*lb*s03-
				10.0*ub*s03-20.0*w3*ub*s03+20.0*s01*ub*s03+120.0*w3*s01*ub*s03+
				60.0*s01*s01*ub*s03-20.0*lb*ub*s03-40.0*w3*lb*ub*s03-80.0*s01*lb*ub*s03+
				30.0*lb*lb*ub*s03+10.0*ub*ub*s03-40.0*w3*ub*ub*s03-80.0*s01*ub*ub*s03+
				30.0*lb*ub*ub*s03+30.0*ub*ub*ub*s03+120.0*s01*s01*s02*s03-20.0*lb*s02*s03-
				120.0*s01*lb*s02*s03+40.0*lb*lb*s02*s03+20.0*ub*s02*s03-120.0*s01*ub*s02*s03+
				40.0*lb*ub*s02*s03+40.0*ub*ub*s02*s03-60.0*s01*s01*s03*s03+10.0*lb*s03*s03+
				60.0*s01*lb*s03*s03-20.0*lb*lb*s03*s03-10.0*ub*s03*s03+60.0*s01*ub*s03*s03-
				20.0*lb*ub*s03*s03-20.0*ub*ub*s03*s03))/(120.*(1.0+w1)*(1.0+w2));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::a2r1(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ar=-((-1.0+lb-ub)*(2.0+2.0*w2+2.0*w3+lb+ub+2.0*s02-2.0*s03))/2.;
			break;
		case 1 : ar=-((-1.0+lb-ub)*(-6.0*s01-6.0*w2*s01-6.0*w3*s01+2.0*lb+3.0*w2*lb+3.0*w3*lb-
				3.0*s01*lb+2.0*lb*lb+4.0*ub+3.0*w2*ub+3.0*w3*ub-3.0*s01*ub+2.0*lb*ub+
				2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-3.0*ub*s03))/(6.*(1.0+w1));
			break;
		case 2 : ar=-((-1.0+lb-ub)*(3.0*w2+3.0*w2*w2-3.0*w3-3.0*w3*w3-lb-3.0*w3*lb-lb*lb-
				2.0*ub-3.0*w3*ub-lb*ub-ub*ub-3.0*s02-6.0*w3*s02-3.0*lb*s02-3.0*ub*s02-
				3.0*s02*s02+3.0*s03+6.0*w3*s03+3.0*lb*s03+3.0*ub*s03+6.0*s02*s03-3.0*s03*s03))/(6.*(1.0+w2));
			break;
		case 3 : ar=-((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01+2.0*lb+
				6.0*w2*lb+6.0*w2*w2*lb-2.0*w3*lb-6.0*w3*w3*lb+4.0*s01*lb+12.0*w3*s01*lb-
				lb*lb-8.0*w3*lb*lb+4.0*s01*lb*lb-3.0*lb*lb*lb-2.0*ub+6.0*w2*ub+
				6.0*w2*w2*ub-10.0*w3*ub-6.0*w3*w3*ub+8.0*s01*ub+12.0*w3*s01*ub-
				4.0*lb*ub-8.0*w3*lb*ub+4.0*s01*lb*ub-3.0*lb*lb*ub-7.0*ub*ub-
				8.0*w3*ub*ub+4.0*s01*ub*ub-3.0*lb*ub*ub-3.0*ub*ub*ub+12.0*s01*s02+
				24.0*w3*s01*s02-2.0*lb*s02-12.0*w3*lb*s02+12.0*s01*lb*s02-8.0*lb*lb*s02-
				10.0*ub*s02-12.0*w3*ub*s02+12.0*s01*ub*s02-8.0*lb*ub*s02-8.0*ub*ub*s02+
				12.0*s01*s02*s02-6.0*lb*s02*s02-6.0*ub*s02*s02-12.0*s01*s03-24.0*w3*s01*s03+
				2.0*lb*s03+12.0*w3*lb*s03-12.0*s01*lb*s03+8.0*lb*lb*s03+10.0*ub*s03+
				12.0*w3*ub*s03-12.0*s01*ub*s03+8.0*lb*ub*s03+8.0*ub*ub*s03-24.0*s01*s02*s03+
				12.0*lb*s02*s03+12.0*ub*s02*s03+12.0*s01*s03*s03-6.0*lb*s03*s03-6.0*ub*s03*s03))/
						(24.*(1.0+w1)*(1.0+w2));
			break;
		case 4 : ar=((-1.0+lb-ub)*(-3.0*w2-3.0*w2*w2+3.0*w3+3.0*w3*w3+lb+3.0*w3*lb+lb*lb+
				2.0*ub+3.0*w3*ub+lb*ub+ub*ub+3.0*s02+6.0*w3*s02+3.0*lb*s02+3.0*ub*s02+
				3.0*s02*s02-3.0*s03-6.0*w3*s03-3.0*lb*s03-3.0*ub*s03-6.0*s02*s03+3.0*s03*s03))/6.;
			break;
		case 5 : ar=((-1.0+lb-ub)*(12.0*w2*s01+12.0*w2*w2*s01-12.0*w3*s01-12.0*w3*w3*s01-2.0*lb-
				6.0*w2*lb-6.0*w2*w2*lb+2.0*w3*lb+6.0*w3*w3*lb-4.0*s01*lb-12.0*w3*s01*lb+
				lb*lb+8.0*w3*lb*lb-4.0*s01*lb*lb+3.0*lb*lb*lb+2.0*ub-6.0*w2*ub-
				6.0*w2*w2*ub+10.0*w3*ub+6.0*w3*w3*ub-8.0*s01*ub-12.0*w3*s01*ub+4.0*lb*ub+
				8.0*w3*lb*ub-4.0*s01*lb*ub+3.0*lb*lb*ub+7.0*ub*ub+8.0*w3*ub*ub-
				4.0*s01*ub*ub+3.0*lb*ub*ub+3.0*ub*ub*ub-12.0*s01*s02-24.0*w3*s01*s02+2.0*lb*s02+
				12.0*w3*lb*s02-12.0*s01*lb*s02+8.0*lb*lb*s02+10.0*ub*s02+12.0*w3*ub*s02-
				12.0*s01*ub*s02+8.0*lb*ub*s02+8.0*ub*ub*s02-12.0*s01*s02*s02+6.0*lb*s02*s02+
				6.0*ub*s02*s02+12.0*s01*s03+24.0*w3*s01*s03-2.0*lb*s03-12.0*w3*lb*s03+
				12.0*s01*lb*s03-8.0*lb*lb*s03-10.0*ub*s03-12.0*w3*ub*s03+12.0*s01*ub*s03-
				8.0*lb*ub*s03-8.0*ub*ub*s03+24.0*s01*s02*s03-12.0*lb*s02*s03-12.0*ub*s02*s03-
				12.0*s01*s03*s03+6.0*lb*s03*s03+6.0*ub*s03*s03))/(24.*(1.0+w1));
			break;
		case 6 : ar=-((-1.0+lb-ub)*(2.0*w2+6.0*w2*w2+4.0*w2*w2*w2+2.0*w3+6.0*w3*w3+4.0*w3*w3*w3+
				4.0*w3*lb+6.0*w3*w3*lb+lb*lb+4.0*w3*lb*lb+lb*lb*lb+2.0*ub+8.0*w3*ub+
				6.0*w3*w3*ub+2.0*lb*ub+4.0*w3*lb*ub+lb*lb*ub+3.0*ub*ub+4.0*w3*ub*ub+
				lb*ub*ub+ub*ub*ub+2.0*s02+12.0*w3*s02+12.0*w3*w3*s02+4.0*lb*s02+
				12.0*w3*lb*s02+4.0*lb*lb*s02+8.0*ub*s02+12.0*w3*ub*s02+4.0*lb*ub*s02+
				4.0*ub*ub*s02+6.0*s02*s02+12.0*w3*s02*s02+6.0*lb*s02*s02+6.0*ub*s02*s02+
				4.0*s02*s02*s02-2.0*s03-12.0*w3*s03-12.0*w3*w3*s03-4.0*lb*s03-12.0*w3*lb*s03-
				4.0*lb*lb*s03-8.0*ub*s03-12.0*w3*ub*s03-4.0*lb*ub*s03-4.0*ub*ub*s03-
				12.0*s02*s03-24.0*w3*s02*s03-12.0*lb*s02*s03-12.0*ub*s02*s03-12.0*s02*s02*s03+
				6.0*s03*s03+12.0*w3*s03*s03+6.0*lb*s03*s03+6.0*ub*s03*s03+12.0*s02*s03*s03-
				4.0*s03*s03*s03))/(12.*(1.0+w2));
			break;
		case 7 : ar=-((-1.0+lb-ub)*(-20.0*w2*s01-60.0*w2*w2*s01-40.0*w2*w2*w2*s01-20.0*w3*s01-
				60.0*w3*w3*s01-40.0*w3*w3*w3*s01-2.0*lb+10.0*w2*lb+30.0*w2*w2*lb+
				20.0*w2*w2*w2*lb-10.0*w3*lb+10.0*w3*w3*lb+20.0*w3*w3*w3*lb-40.0*w3*s01*lb-
				60.0*w3*w3*s01*lb-7.0*lb*lb+10.0*w3*lb*lb+40.0*w3*w3*lb*lb-
				10.0*s01*lb*lb-40.0*w3*s01*lb*lb+3.0*lb*lb*lb+30.0*w3*lb*lb*lb-10.0*s01*lb*lb*lb+
				8.0*lb*lb*lb*lb+2.0*ub+10.0*w2*ub+30.0*w2*w2*ub+20.0*w2*w2*w2*ub+30.0*w3*ub+
				50.0*w3*w3*ub+20.0*w3*w3*w3*ub-20.0*s01*ub-80.0*w3*s01*ub-60.0*w3*w3*s01*ub+
				4.0*lb*ub+40.0*w3*lb*ub+40.0*w3*w3*lb*ub-20.0*s01*lb*ub-
				40.0*w3*s01*lb*ub+11.0*lb*lb*ub+30.0*w3*lb*lb*ub-10.0*s01*lb*lb*ub+
				8.0*lb*lb*lb*ub+23.0*ub*ub+70.0*w3*ub*ub+40.0*w3*w3*ub*ub-30.0*s01*ub*ub-
				40.0*w3*s01*ub*ub+19.0*lb*ub*ub+30.0*w3*lb*ub*ub-10.0*s01*lb*ub*ub+
				8.0*lb*lb*ub*ub+27.0*ub*ub*ub+30.0*w3*ub*ub*ub-10.0*s01*ub*ub*ub+8.0*lb*ub*ub*ub+
				8.0*ub*ub*ub*ub-20.0*s01*s02-120.0*w3*s01*s02-120.0*w3*w3*s01*s02-10.0*lb*s02+
				20.0*w3*lb*s02+60.0*w3*w3*lb*s02-40.0*s01*lb*s02-120.0*w3*s01*lb*s02+
				10.0*lb*lb*s02+80.0*w3*lb*lb*s02-40.0*s01*lb*lb*s02+30.0*lb*lb*lb*s02+
				30.0*ub*s02+100.0*w3*ub*s02+60.0*w3*w3*ub*s02-80.0*s01*ub*s02-
				120.0*w3*s01*ub*s02+40.0*lb*ub*s02+80.0*w3*lb*ub*s02-40.0*s01*lb*ub*s02+
				30.0*lb*lb*ub*s02+70.0*ub*ub*s02+80.0*w3*ub*ub*s02-40.0*s01*ub*ub*s02+
				30.0*lb*ub*ub*s02+30.0*ub*ub*ub*s02-60.0*s01*s02*s02-120.0*w3*s01*s02*s02+
				10.0*lb*s02*s02+60.0*w3*lb*s02*s02-60.0*s01*lb*s02*s02+40.0*lb*lb*s02*s02+
				50.0*ub*s02*s02+60.0*w3*ub*s02*s02-60.0*s01*ub*s02*s02+40.0*lb*ub*s02*s02+
				40.0*ub*ub*s02*s02-40.0*s01*s02*s02*s02+20.0*lb*s02*s02*s02+20.0*ub*s02*s02*s02+20.0*s01*s03+
				120.0*w3*s01*s03+120.0*w3*w3*s01*s03+10.0*lb*s03-20.0*w3*lb*s03-
				60.0*w3*w3*lb*s03+40.0*s01*lb*s03+120.0*w3*s01*lb*s03-10.0*lb*lb*s03-
				80.0*w3*lb*lb*s03+40.0*s01*lb*lb*s03-30.0*lb*lb*lb*s03-30.0*ub*s03-
				100.0*w3*ub*s03-60.0*w3*w3*ub*s03+80.0*s01*ub*s03+120.0*w3*s01*ub*s03-
				40.0*lb*ub*s03-80.0*w3*lb*ub*s03+40.0*s01*lb*ub*s03-30.0*lb*lb*ub*s03-
				70.0*ub*ub*s03-80.0*w3*ub*ub*s03+40.0*s01*ub*ub*s03-30.0*lb*ub*ub*s03-
				30.0*ub*ub*ub*s03+120.0*s01*s02*s03+240.0*w3*s01*s02*s03-20.0*lb*s02*s03-
				120.0*w3*lb*s02*s03+120.0*s01*lb*s02*s03-80.0*lb*lb*s02*s03-100.0*ub*s02*s03-
				120.0*w3*ub*s02*s03+120.0*s01*ub*s02*s03-80.0*lb*ub*s02*s03-80.0*ub*ub*s02*s03+
				120.0*s01*s02*s02*s03-60.0*lb*s02*s02*s03-60.0*ub*s02*s02*s03-60.0*s01*s03*s03-
				120.0*w3*s01*s03*s03+10.0*lb*s03*s03+60.0*w3*lb*s03*s03-60.0*s01*lb*s03*s03+
				40.0*lb*lb*s03*s03+50.0*ub*s03*s03+60.0*w3*ub*s03*s03-60.0*s01*ub*s03*s03+
				40.0*lb*ub*s03*s03+40.0*ub*ub*s03*s03-120.0*s01*s02*s03*s03+60.0*lb*s02*s03*s03+
				60.0*ub*s02*s03*s03+40.0*s01*s03*s03*s03-20.0*lb*s03*s03*s03-20.0*ub*s03*s03*s03))/(120.*(1.0+w1)*(1.0+w2));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::a2r2(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ar=(1.0+2.0*w2)*(1.0-lb+ub);
			break;
		case 1 : ar=-((1.0+2.0*w2)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/(2.*(1.0+w1));
			break;
		case 2 : ar=0.0;
			break;
		case 3 : ar=0.0;
			break;
		case 4 : ar=0.0;
			break;
		case 5 : ar=0.0;
			break;
		case 6 : ar=((1.0+2.0*w2)*(w2+w2*w2)*(1.0-lb+ub))/(3.*(1.0+w2));
			break;
		case 7 : ar=-((1.0+2.0*w2)*(w2+w2*w2)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/(6.*(1.0+w1)*(1.0+w2));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::a2r3(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ar=(1.0+2.0*w3)*(1.0-lb+ub);
			break;
		case 1 : ar=-((1.0+2.0*w3)*(-1.0+lb-ub)*(-2.0*s01+lb+ub))/(2.*(1.0+w1));
			break;
		case 2 : ar=((1.0+2.0*w3)*(-1.0+lb-ub)*(lb+ub+2.0*s02-2.0*s03))/(2.*(1.0+w2));
			break;
		case 3 : ar=((1.0+2.0*w3)*(-1.0+lb-ub)*(-lb-3.0*s01*lb+2.0*lb*lb+ub-3.0*s01*ub+
				2.0*lb*ub+2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-
				3.0*ub*s03))/(6.*(1.0+w1)*(1.0+w2));
			break;
		case 4 : ar=((1.0+2.0*w3)*(-1.0+lb-ub)*(lb+ub+2.0*s02-2.0*s03))/2.;
			break;
		case 5 : ar=((1.0+2.0*w3)*(-1.0+lb-ub)*(-lb-3.0*s01*lb+2.0*lb*lb+ub-3.0*s01*ub+
				2.0*lb*ub+2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-
				3.0*ub*s03))/(6.*(1.0+w1));
			break;
		case 6 : ar=-((1.0+2.0*w3)*(-1.0+lb-ub)*(2.0*w3+2.0*w3*w3-lb+2.0*lb*lb+ub+2.0*lb*ub+
				2.0*ub*ub+6.0*lb*s02+6.0*ub*s02+6.0*s02*s02-6.0*lb*s03-6.0*ub*s03-
				12.0*s02*s03+6.0*s03*s03))/(6.*(1.0+w2));
			break;
		case 7 : ar=-((1.0+2.0*w3)*(-1.0+lb-ub)*(-4.0*w3*s01-4.0*w3*w3*s01+2.0*w3*lb+2.0*w3*w3*lb+
				2.0*s01*lb-3.0*lb*lb-4.0*s01*lb*lb+3.0*lb*lb*lb+2.0*w3*ub+2.0*w3*w3*ub-
				2.0*s01*ub-4.0*s01*lb*ub+3.0*lb*lb*ub+3.0*ub*ub-4.0*s01*ub*ub+
				3.0*lb*ub*ub+3.0*ub*ub*ub-4.0*lb*s02-12.0*s01*lb*s02+8.0*lb*lb*s02+4.0*ub*s02-
				12.0*s01*ub*s02+8.0*lb*ub*s02+8.0*ub*ub*s02-12.0*s01*s02*s02+6.0*lb*s02*s02+
				6.0*ub*s02*s02+4.0*lb*s03+12.0*s01*lb*s03-8.0*lb*lb*s03-4.0*ub*s03+
				12.0*s01*ub*s03-8.0*lb*ub*s03-8.0*ub*ub*s03+24.0*s01*s02*s03-12.0*lb*s02*s03-
				12.0*ub*s02*s03-12.0*s01*s03*s03+6.0*lb*s03*s03+6.0*ub*s03*s03))/(12.*(1.0+w1)*(1.0+w2));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::a2r4(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ar=((-1.0+lb-ub)*(-2.0-2.0*w2-2.0*w3+lb+ub+2.0*s02-2.0*s03))/2.;
			break;
		case 1 : ar=((-1.0+lb-ub)*(6.0*s01+6.0*w2*s01+6.0*w3*s01-4.0*lb-3.0*w2*lb-3.0*w3*lb-
				3.0*s01*lb+2.0*lb*lb-2.0*ub-3.0*w2*ub-3.0*w3*ub-3.0*s01*ub+2.0*lb*ub+
				2.0*ub*ub-6.0*s01*s02+3.0*lb*s02+3.0*ub*s02+6.0*s01*s03-3.0*lb*s03-3.0*ub*s03))/(6.*(1.0+w1));
			break;
		case 2 : ar=((-1.0+lb-ub)*(3.0*w2+3.0*w2*w2-3.0*w3-3.0*w3*w3+2.0*lb+3.0*w3*lb-lb*lb+
				ub+3.0*w3*ub-lb*ub-ub*ub+3.0*s02+6.0*w3*s02-3.0*lb*s02-3.0*ub*s02-
				3.0*s02*s02-3.0*s03-6.0*w3*s03+3.0*lb*s03+3.0*ub*s03+6.0*s02*s03-3.0*s03*s03))/(6.*(1.0+w2));
			break;
		case 3 : ar=((-1.0+lb-ub)*(-12.0*w2*s01-12.0*w2*w2*s01+12.0*w3*s01+12.0*w3*w3*s01-2.0*lb+
				6.0*w2*lb+6.0*w2*w2*lb-10.0*w3*lb-6.0*w3*w3*lb-8.0*s01*lb-12.0*w3*s01*lb+
				7.0*lb*lb+8.0*w3*lb*lb+4.0*s01*lb*lb-3.0*lb*lb*lb+2.0*ub+6.0*w2*ub+
				6.0*w2*w2*ub-2.0*w3*ub-6.0*w3*w3*ub-4.0*s01*ub-12.0*w3*s01*ub+4.0*lb*ub+
				8.0*w3*lb*ub+4.0*s01*lb*ub-3.0*lb*lb*ub+ub*ub+8.0*w3*ub*ub+
				4.0*s01*ub*ub-3.0*lb*ub*ub-3.0*ub*ub*ub-12.0*s01*s02-24.0*w3*s01*s02+
				10.0*lb*s02+12.0*w3*lb*s02+12.0*s01*lb*s02-8.0*lb*lb*s02+2.0*ub*s02+
				12.0*w3*ub*s02+12.0*s01*ub*s02-8.0*lb*ub*s02-8.0*ub*ub*s02+12.0*s01*s02*s02-
				6.0*lb*s02*s02-6.0*ub*s02*s02+12.0*s01*s03+24.0*w3*s01*s03-10.0*lb*s03-
				12.0*w3*lb*s03-12.0*s01*lb*s03+8.0*lb*lb*s03-2.0*ub*s03-12.0*w3*ub*s03-
				12.0*s01*ub*s03+8.0*lb*ub*s03+8.0*ub*ub*s03-24.0*s01*s02*s03+12.0*lb*s02*s03+
				12.0*ub*s02*s03+12.0*s01*s03*s03-6.0*lb*s03*s03-6.0*ub*s03*s03))/(24.*(1.0+w1)*(1.0+w2));
			break;
		case 4 : ar=-((-1.0+lb-ub)*(-3.0*w2-3.0*w2*w2+3.0*w3+3.0*w3*w3-2.0*lb-3.0*w3*lb+
				lb*lb-ub-3.0*w3*ub+lb*ub+ub*ub-3.0*s02-6.0*w3*s02+3.0*lb*s02+
				3.0*ub*s02+3.0*s02*s02+3.0*s03+6.0*w3*s03-3.0*lb*s03-3.0*ub*s03-6.0*s02*s03+
				3.0*s03*s03))/6.;
			break;
		case 5 : ar=-((-1.0+lb-ub)*(12.0*w2*s01+12.0*w2*w2*s01-12.0*w3*s01-12.0*w3*w3*s01+2.0*lb-
				6.0*w2*lb-6.0*w2*w2*lb+10.0*w3*lb+6.0*w3*w3*lb+8.0*s01*lb+
				12.0*w3*s01*lb-7.0*lb*lb-8.0*w3*lb*lb-4.0*s01*lb*lb+3.0*lb*lb*lb-2.0*ub-
				6.0*w2*ub-6.0*w2*w2*ub+2.0*w3*ub+6.0*w3*w3*ub+4.0*s01*ub+12.0*w3*s01*ub-
				4.0*lb*ub-8.0*w3*lb*ub-4.0*s01*lb*ub+3.0*lb*lb*ub-ub*ub-8.0*w3*ub*ub-
				4.0*s01*ub*ub+3.0*lb*ub*ub+3.0*ub*ub*ub+12.0*s01*s02+24.0*w3*s01*s02-
				10.0*lb*s02-12.0*w3*lb*s02-12.0*s01*lb*s02+8.0*lb*lb*s02-2.0*ub*s02-
				12.0*w3*ub*s02-12.0*s01*ub*s02+8.0*lb*ub*s02+8.0*ub*ub*s02-12.0*s01*s02*s02+
				6.0*lb*s02*s02+6.0*ub*s02*s02-12.0*s01*s03-24.0*w3*s01*s03+10.0*lb*s03+
				12.0*w3*lb*s03+12.0*s01*lb*s03-8.0*lb*lb*s03+2.0*ub*s03+12.0*w3*ub*s03+
				12.0*s01*ub*s03-8.0*lb*ub*s03-8.0*ub*ub*s03+24.0*s01*s02*s03-12.0*lb*s02*s03-
				12.0*ub*s02*s03-12.0*s01*s03*s03+6.0*lb*s03*s03+6.0*ub*s03*s03))/(24.*(1.0+w1));
			break;
		case 6 : ar=-((-1.0+lb-ub)*(2.0*w2+6.0*w2*w2+4.0*w2*w2*w2+2.0*w3+6.0*w3*w3+4.0*w3*w3*w3-
				2.0*lb-8.0*w3*lb-6.0*w3*w3*lb+3.0*lb*lb+4.0*w3*lb*lb-lb*lb*lb-
				4.0*w3*ub-6.0*w3*w3*ub+2.0*lb*ub+4.0*w3*lb*ub-lb*lb*ub+ub*ub+
				4.0*w3*ub*ub-lb*ub*ub-ub*ub*ub-2.0*s02-12.0*w3*s02-12.0*w3*w3*s02+
				8.0*lb*s02+12.0*w3*lb*s02-4.0*lb*lb*s02+4.0*ub*s02+12.0*w3*ub*s02-
				4.0*lb*ub*s02-4.0*ub*ub*s02+6.0*s02*s02+12.0*w3*s02*s02-6.0*lb*s02*s02-
				6.0*ub*s02*s02-4.0*s02*s02*s02+2.0*s03+12.0*w3*s03+12.0*w3*w3*s03-8.0*lb*s03-
				12.0*w3*lb*s03+4.0*lb*lb*s03-4.0*ub*s03-12.0*w3*ub*s03+4.0*lb*ub*s03+
				4.0*ub*ub*s03-12.0*s02*s03-24.0*w3*s02*s03+12.0*lb*s02*s03+12.0*ub*s02*s03+
				12.0*s02*s02*s03+6.0*s03*s03+12.0*w3*s03*s03-6.0*lb*s03*s03-6.0*ub*s03*s03-
				12.0*s02*s03*s03+4.0*s03*s03*s03))/(12.*(1.0+w2));
			break;
		case 7 : ar=-((-1.0+lb-ub)*(-20.0*w2*s01-60.0*w2*w2*s01-40.0*w2*w2*w2*s01-20.0*w3*s01-
				60.0*w3*w3*s01-40.0*w3*w3*w3*s01+2.0*lb+10.0*w2*lb+30.0*w2*w2*lb+
				20.0*w2*w2*w2*lb+30.0*w3*lb+50.0*w3*w3*lb+20.0*w3*w3*w3*lb+20.0*s01*lb+
				80.0*w3*s01*lb+60.0*w3*w3*s01*lb-23.0*lb*lb-70.0*w3*lb*lb-
				40.0*w3*w3*lb*lb-30.0*s01*lb*lb-40.0*w3*s01*lb*lb+27.0*lb*lb*lb+
				30.0*w3*lb*lb*lb+10.0*s01*lb*lb*lb-8.0*lb*lb*lb*lb-2.0*ub+10.0*w2*ub+30.0*w2*w2*ub+
				20.0*w2*w2*w2*ub-10.0*w3*ub+10.0*w3*w3*ub+20.0*w3*w3*w3*ub+40.0*w3*s01*ub+
				60.0*w3*w3*s01*ub-4.0*lb*ub-40.0*w3*lb*ub-40.0*w3*w3*lb*ub-
				20.0*s01*lb*ub-40.0*w3*s01*lb*ub+19.0*lb*lb*ub+30.0*w3*lb*lb*ub+
				10.0*s01*lb*lb*ub-8.0*lb*lb*lb*ub+7.0*ub*ub-10.0*w3*ub*ub-
				40.0*w3*w3*ub*ub-10.0*s01*ub*ub-40.0*w3*s01*ub*ub+11.0*lb*ub*ub+
				30.0*w3*lb*ub*ub+10.0*s01*lb*ub*ub-8.0*lb*lb*ub*ub+3.0*ub*ub*ub+
				30.0*w3*ub*ub*ub+10.0*s01*ub*ub*ub-8.0*lb*ub*ub*ub-8.0*ub*ub*ub*ub+20.0*s01*s02+
				120.0*w3*s01*s02+120.0*w3*w3*s01*s02-30.0*lb*s02-100.0*w3*lb*s02-
				60.0*w3*w3*lb*s02-80.0*s01*lb*s02-120.0*w3*s01*lb*s02+70.0*lb*lb*s02+
				80.0*w3*lb*lb*s02+40.0*s01*lb*lb*s02-30.0*lb*lb*lb*s02+10.0*ub*s02-
				20.0*w3*ub*s02-60.0*w3*w3*ub*s02-40.0*s01*ub*s02-120.0*w3*s01*ub*s02+
				40.0*lb*ub*s02+80.0*w3*lb*ub*s02+40.0*s01*lb*ub*s02-30.0*lb*lb*ub*s02+
				10.0*ub*ub*s02+80.0*w3*ub*ub*s02+40.0*s01*ub*ub*s02-30.0*lb*ub*ub*s02-
				30.0*ub*ub*ub*s02-60.0*s01*s02*s02-120.0*w3*s01*s02*s02+50.0*lb*s02*s02+
				60.0*w3*lb*s02*s02+60.0*s01*lb*s02*s02-40.0*lb*lb*s02*s02+10.0*ub*s02*s02+
				60.0*w3*ub*s02*s02+60.0*s01*ub*s02*s02-40.0*lb*ub*s02*s02-40.0*ub*ub*s02*s02+
				40.0*s01*s02*s02*s02-20.0*lb*s02*s02*s02-20.0*ub*s02*s02*s02-20.0*s01*s03-120.0*w3*s01*s03-
				120.0*w3*w3*s01*s03+30.0*lb*s03+100.0*w3*lb*s03+60.0*w3*w3*lb*s03+
				80.0*s01*lb*s03+120.0*w3*s01*lb*s03-70.0*lb*lb*s03-80.0*w3*lb*lb*s03-
				40.0*s01*lb*lb*s03+30.0*lb*lb*lb*s03-10.0*ub*s03+20.0*w3*ub*s03+
				60.0*w3*w3*ub*s03+40.0*s01*ub*s03+120.0*w3*s01*ub*s03-40.0*lb*ub*s03-
				80.0*w3*lb*ub*s03-40.0*s01*lb*ub*s03+30.0*lb*lb*ub*s03-10.0*ub*ub*s03-
				80.0*w3*ub*ub*s03-40.0*s01*ub*ub*s03+30.0*lb*ub*ub*s03+30.0*ub*ub*ub*s03+
				120.0*s01*s02*s03+240.0*w3*s01*s02*s03-100.0*lb*s02*s03-120.0*w3*lb*s02*s03-
				120.0*s01*lb*s02*s03+80.0*lb*lb*s02*s03-20.0*ub*s02*s03-120.0*w3*ub*s02*s03-
				120.0*s01*ub*s02*s03+80.0*lb*ub*s02*s03+80.0*ub*ub*s02*s03-120.0*s01*s02*s02*s03+
				60.0*lb*s02*s02*s03+60.0*ub*s02*s02*s03-60.0*s01*s03*s03-120.0*w3*s01*s03*s03+
				50.0*lb*s03*s03+60.0*w3*lb*s03*s03+60.0*s01*lb*s03*s03-40.0*lb*lb*s03*s03+
				10.0*ub*s03*s03+60.0*w3*ub*s03*s03+60.0*s01*ub*s03*s03-40.0*lb*ub*s03*s03-
				40.0*ub*ub*s03*s03+120.0*s01*s02*s03*s03-60.0*lb*s02*s03*s03-60.0*ub*s02*s03*s03-
				40.0*s01*s03*s03*s03+20.0*lb*s03*s03*s03+20.0*ub*s03*s03*s03))/(120.*(1.0+w1)*(1.0+w2));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::ae(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2;

	switch(p) {
		case 0 : ar=1.0-lb+ub;
			break;
		case 1 : ar=-((-1.0+lb-ub)*(lb+ub-2.0*s01))/(2.*(1.0+w1));
			break;
		case 2 : ar=-((-1.0+lb-ub)*(lb+ub-2.0*s01))/2.;
			break;
		case 3 : ar=-((-1.0+lb-ub)*(-lb+2.0*lb*lb+ub+2.0*lb*ub+2.0*ub*ub-6.0*lb*s01-6.0*ub*s01+6.0*s01*s01))/
				(6.*(1.0+w1));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::am(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2, sM = s02;

	switch(p) {
		case 0 : ar=1.0-lb+ub;
			break;
		case 1 : ar=-((-1.0+lb-ub)*(lb+ub-2.0*s01))/(2.*(1.0+w1));
			break;
		case 2 : ar=(1.0-lb+ub)*(sM-s02);
			break;
		case 3 : ar=-((-1.0+lb-ub)*(lb+ub-2.0*s01)*(sM-s02))/(2.*(1.0+w1));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}

double SuperCluster::ad(int p, double s01, double s02, double s03, double w1, double w2,
		double w3, int x1, int x2) {
	if(x1 > x2) return 0.0;
	double ar = 0.0, lb = (double) x1, ub = (double) x2, sM = s02;

	switch(p) {
		case 0 : ar=1.0-lb+ub;
			break;
		case 1 : ar=-((-1.0+lb-ub)*(lb+ub-2.0*s01))/(2.*(1.0+w1));
			break;
		case 2 : ar=-((-1.0+lb-ub)*(lb+ub-2.0*sM-2.0*s03))/2.;
			break;
		case 3 : ar=-((-1.0+lb-ub)*(-lb+2.0*lb*lb+ub+2.0*lb*ub+2.0*ub*ub-3.0*lb*s01-
				3.0*ub*s01-3.0*lb*sM-3.0*ub*sM+6.0*s01*sM-3.0*lb*s03-3.0*ub*s03+6.0*s01*s03))/(6.*(1.0+w1));
			break;
		default : ar = 0.0;
			break;
	}

	return ar;
}