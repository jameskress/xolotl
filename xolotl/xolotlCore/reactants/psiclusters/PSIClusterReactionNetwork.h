#ifndef PSI_CLUSTER_REACTION_NETWORK_H
#define PSI_CLUSTER_REACTION_NETWORK_H

// Includes
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <ReactionNetwork.h>
#include <PSICluster.h>

namespace xolotlCore {

/**
 *  This is a simple convenience class that contains a set of Reactants and the
 *  properties that describe that set.
 */
class PSIClusterReactionNetwork: public ReactionNetwork {

private:

	/**
	 * This structure compares two PSIClusters
	 */
	struct PSIClusterComparator {
		bool operator()(const std::map<std::string, int>& lhs,
				const std::map<std::string, int>& rhs) const {

			// Local Declarations
			int numHe_lhs = 0, numV_lhs = 0, numI_lhs = 0;
			int numHe_rhs = 0, numV_rhs = 0, numI_rhs = 0;

			// Get the cluster sizes
			numHe_lhs = lhs.at("numHeClusters");
			numV_lhs = lhs.at("numVClusters");
			numI_lhs = lhs.at("numIClusters");
			numHe_rhs = rhs.at("numHeClusters");
			numV_rhs = rhs.at("numVClusters");
			numI_rhs = rhs.at("numIClusters");

			return lhs < rhs;
		}
	};

	/**
	 * The map of single-species clusters, indexed by a map that contains the
	 * name of the reactant and its size.
	 */
	std::map<std::map<std::string, int>, std::shared_ptr<PSICluster>> singleSpeciesMap;

	/**
	 * The map of mixed or compound species clusters, indexed by a map that
	 * contains the name of the constituents of the compound reactant and their
	 * sizes.
	 */
	std::map<std::map<std::string, int>, std::shared_ptr<PSICluster>> mixedSpeciesMap;

public:

	/**
	 * The Constructor
	 */
	PSIClusterReactionNetwork();

	/**
	 * The copy constructor
	 * @param other
	 */
	PSIClusterReactionNetwork(const PSIClusterReactionNetwork &other);

	/**
	 * Converts an cluster index (found in the `reactants` vector)
	 * to a map describing the cluster's 
	 *
	 * @returns a map with `speciesLabel` => `quantity`
	 */
	std::map<std::string, int> toClusterMap(int index) const;

	/**
	 * Converts an cluster map (with `speciesLabel` => `quantity`)
	 * to the index corresponding to its position in the reactants vector
	 * @param the map of species labels to species size that describes the
	 * desired cluster, i.e. "He",1 or "He",1;"V",2.
	 * @returns the index of the cluster or -1 if it could not be found
	 */
	int toClusterIndex(std::map<std::string, int> clusterMap) const;

	/**
	 * This operation returns a reactant with the given name and size if it
	 * exists in the network or null if not.
	 * @param name the name of the reactant
	 * @param size the size of the reactant
	 * @return A shared pointer to the reactant
	 */
	const std::shared_ptr<Reactant> & get(std::string name, int size);

	/**
	 * This operation returns a compound reactant with the given name and size if it
	 * exists in the network or null if not.
	 * @param name the name of the compound reactant
	 * @param sizes an array containing the sizes of each piece of the reactant
	 * @return A shared pointer to the compound reactant
	 */
	const std::shared_ptr<Reactant> & getCompound(std::string name,
			std::vector<int> sizes);

	/**
	 * This operation adds a reactant or a compound reactant to the network.
	 * The reactant will not be added to the network if the PSICluster does
	 * not recognize it as a type of reactant that it cares about (including
	 * adding null).
	 * @param reactant The reactant that should be added to the network.
	 */
	void add(const std::shared_ptr<Reactant> & reactant);

	/**
	 * This operation returns the names of the reactants in the network. For a
	 * PSIClusterReactionNetwork, these are He, V, I, HeV, HeI.
	 * @return A vector with one entry for each of the distinct reactant types
	 * in the network.
	 */
	const std::vector<std::string> & getNames();

	/**
	 * This operation returns the names of the compound reactants in the
	 * network.
	 * @return A vector with one each for each of the distinct compound
	 * reactant types in the network.
	 */
	const std::vector<std::string> & getCompoundNames();

	/**
	 * This operation returns a map of the properties of this reaction network.
	 * @return The map of properties that has been configured for this
	 * ReactionNetwork.
	 *
	 * The PSIClusterReactionNetwork always has the following properties:
	 * > maxHeClusterSize - The number of He atoms in the largest single-species
	 *  He cluster.
	 * > maxVClusterSize - The number of atomic vacancies in the largest
	 * single-species V cluster.
	 * > maxIClusterSize - The number of interstitials in the largest
	 * single-species I cluster.
	 * > maxHeVClusterSize - The number of species of all types in the largest
	 * mixed species in the network. It is equal to the sum of the max single
	 * species helium and vacancy cluster sizes by default.
	 * > maxHeIClusterSize - The number of species of all types in the largest
	 * mixed species in the network. It is equal to the sum of the max single
	 * species helium and vacancy cluster sizes by default.
	 * > numHeClusters - The number of single-species He clusters of all sizes in
	 * the network.
	 * > numVClusters - The number of single-species V clusters of all sizes in the
	 * network.
	 * > numIClusters - The number of single-species I clusters of all sizes in the
	 * network.
	 * > numHeVClusters - The number of HeV clusters of all sizes in the
	 * network.
	 * > numHeIClusters - The number of HeI clusters of all sizes in the
	 * network.
	 *
	 * These properties are always updated when a cluster is added.
	 */
	const std::map<std::string, std::string> & getProperties();

	/**
	 * This operation sets a property with the given key to the specified value
	 * for the network. ReactionNetworks may reserve the right to ignore this
	 * operation for special key types.
	 * @param key The key for the property
	 * @param value The value to which the key should be set.
	 */
	void setProperty(const std::string key, const std::string value);

};

}

#endif