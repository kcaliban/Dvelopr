/* Copyright 2019 Fabian Krause
 *
 * Genetic algorithm functionality using
 *  GenoType: A class to define properties of an individual, e.g. std::string
 *            for AA sequence
 *  Genome: A class to define recombination and mutation of individuals
 *  FitnessFunction: A class that defines how fitness is calculated
*/
#ifndef SRC_LIB_GENALGINST_H_
#define SRC_LIB_GENALGINST_H_
#include "Genome.h"
#include "FitnessFunction.h"
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include <fstream>
template <typename GenoType, typename Genome, typename FitnessFunction>
class GenAlgInst {
 public:
    GenAlgInst(std::mt19937 * mt1) {
      mt = mt1;
    }

    /* simulate(...):
     *
     * Performs genetic algorithm for n steps
    */
    void simulate(Genome genome,
                  FitnessFunction fitnessfunc,
                  std::vector<GenoType> genotype,
                  int n,
                  float mutateProb,
                  float copy,
                  bool debug = false,
                  bool entropy = true,
                  const char * entropyFile = "entropy") {
      std::vector<GenoType> newGen = genotype;
      for (int i = 0; i < n; i++) {
        if (debug) {
          std::cout << "Generation: " << (i + 1) << std::endl;
        }
        newGen = nextGen(genome, fitnessfunc, newGen, mutateProb, copy,
                          debug, entropy, entropyFile);
      }
    }

    /* nextGen(...):
     *
     * Performs genetic algorithm procedure for one step
    */
    std::vector<GenoType> nextGen(Genome genome,
                                  FitnessFunction fitnessfunc,
                                  std::vector<GenoType> genotypes,
                                  float mutateProb,
                                  float copy,
                                  bool debug = false,
                                  bool entropy = true,
                                  const char * entropyFile = "entropy") {
      std::vector<GenoType> newGen;
      // Required for selection and recombination
      std::vector<float> fitnesses;
      if (debug) {
        std::cout << "\tCalculating fitnesses..." << std::endl;
      }
      for (unsigned int i = 0; i < genotypes.size(); i++) {
        fitnesses.push_back(0.0);
      }
      for (unsigned int i = 0; i < genotypes.size(); i++) {
        float fitness = fitnessfunc.calculateFitness(genotypes.at(i));
        fitnesses.at(i) = fitness;
      }
      // SELECTION
      // Requires them to be sorted; we need to keep the original order
      // of elements to associate them with genotypes vector (could also
      // use pairs)
      if (debug) {
        std::cout << "\tSelection..." << std::endl;
      }
      std::vector<size_t> sortedindices(fitnesses.size());
      std::iota(sortedindices.begin(), sortedindices.end(), 0);
      sort(sortedindices.begin(), sortedindices.end(),
          [fitnesses](size_t i1, size_t i2) {
            return fitnesses[i1] > fitnesses[i2];});
      unsigned int amount = static_cast<int>((copy * genotypes.size()));
      for (unsigned int i = 0; i < amount; i++) {
        newGen.push_back(genotypes[sortedindices[i]]);
      }
      if (debug) {
        std::cout << std::endl;
        std::cout << "\tBest individual: " << genotypes[sortedindices[0]]
                  << ", " << fitnesses[sortedindices[0]]
                  << std::endl;
      }
      // RECOMBINATION
      // Generate a discrete random distribution for fitnesses, i.e.
      // each element has prob. of its weight (fitness) divided by the sum
      // of all weights
      if (debug) {
        std::cout << "\tRecombination..." << std::endl;
      }
      // Discrete distribution: p(i) = w_i / sum(w_i),
      // here fitness divided by sum of all fitnesses
      std::discrete_distribution<int> fitnessdistribution(fitnesses.begin(),
                                                          fitnesses.end());
      // Pick two genotypes randomly and recombine
      // until we have a population as big as the initial
      while (amount < genotypes.size()) {
        if (debug) {
          std::cout << "\t\tPopulation size: " << amount << std::endl;
        }
        GenoType inda = genotypes[fitnessdistribution(*mt)];
        GenoType indb = genotypes[fitnessdistribution(*mt)];
        newGen.push_back(genome.crossOver(inda, indb));
        amount++;
      }
      // MUTATION
      // With probability mutateProb mutate every individual
      // Use a uniform distribution between 0 & 1, if random generated number
      // is <= mutateProb do a mutation
      if (debug) {
        std::cout << "\tMutation..." << std::endl;
      }
      std::uniform_real_distribution<float> uniformdistribution(0.0, 1.0);
      for (unsigned int i = 0; i < newGen.size(); i++) {
        if (uniformdistribution(*mt) <= mutateProb) {
          GenoType mutatedGen = genome.mutate(newGen[i]);
          // Insert the new one
          newGen.at(i) = mutatedGen;
          // delete old;
        }
      }
      if (entropy) {
          std::ofstream outfile;

          outfile.open(entropyFile, std::ios::out | std::ios::app);
          outfile.close();
          outfile.open(entropyFile, std::ios_base::app);
          outfile << calculateEntropy(newGen) << "\t"
                  << fitnesses[sortedindices[0]] << std::endl;
      }

      // std::move?
      return newGen;
    }

 private:
    std::mt19937 * mt;

    /* calculateEntropy(vector of genotypes):
     *
     * Returns number of different individuals in generation,
     * requires a "==" relation on GenoType
    */
    int calculateEntropy(std::vector<GenoType> genotypes) {
      unsigned int entropy = 0;
      // O(n log n) could be possible with sort and unique, but then
      // would have to require '<' relation on GenoType
      // Naive O(n^2) Worst Case implementation
      for (unsigned int i = 0; i < genotypes.size(); i++) {
        bool unique = true;
        for (unsigned int j = i + 1; j < genotypes.size(); j++) {
          if (genotypes[i] == genotypes[j]) {
            unique = false;
            break;
          }
        }
        if (unique) {
          entropy++;
        }
      }
      return entropy;
    }
};

#endif  // SRC_LIB_GENALGINST_H_
