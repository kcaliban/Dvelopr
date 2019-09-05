#ifndef GENMA
#define GENMA
#include "Genome.h"
#include "FitnessFunction.h"
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <omp.h>
template <typename GenoType, typename Genome, typename FitnessFunction>
class GenAlgInst
{
  public:
    void simulate(Genome genome, FitnessFunction fitnessfunc,
                                     std::vector<GenoType> genotype, int n,
                                     float mutateProb, bool debug=true,
                                     bool entropy=true,
                                     const char * entropyFile="entropy") {
      std::vector<GenoType> newGen = genotype;
      for (int i = 0; i < n; i++) {
        if (debug) {
          std::cout << "Generation: " << (i + 1) << std::endl;
        }
        // Will the old gen be deleted automatically?
        newGen = nextGen(genome, fitnessfunc, newGen, mutateProb,
                          debug, entropy, entropyFile);
      }
      // Print result at the end
      std::cout << "Individuals in the last population" << std::endl;
      for (unsigned int i = 0; i < genotype.size(); i++) {
        std::cout << "\t\t" << newGen.at(i) << std::endl;
      }
    };

    std::vector<GenoType> nextGen(Genome genome, FitnessFunction fitnessfunc,
                                    std::vector<GenoType> genotypes,
                                    float mutateProb, bool debug=true,
                                    bool entropy=true, const char * entropyFile="entropy") {
      std::vector<GenoType> newGen;
      // Required for selection and recombination
      std::vector<float> fitnesses;
      if (debug) {
        std::cout << "\tCalculating fitnesses..." << std::endl;
      }
      // Initialize vector so that we can use parallelization for fitness calculation
      for (unsigned int i = 0; i < genotypes.size(); i++) {
        fitnesses.push_back(0.0);
      }
      // #pragma omp parallel
      // #pragma omp for
      for (unsigned int i = 0; i < genotypes.size(); i++) {
        float fitness = fitnessfunc.calculateFitness(genotypes.at(i));
        fitnesses.at(i) = fitness;
      }
      /*
      for (GenoType genotype : genotypes) {
        float fitness = fitnessfunc.calculateFitness(genotype);
        fitnesses.push_back(fitness);
      }
      */
      // SELECTION (this should become "modular" aswell)
      //           (the way selection and recombination is done should be
      //            defined elsewhere)
      // Copy the top 20%
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
      unsigned int amount = (int) (0.1 * genotypes.size());
      for (unsigned int i = 0; i < amount; i++) {
        newGen.push_back(genotypes[sortedindices[i]]);
      }
      if (debug) {
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
      unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
      std::default_random_engine generator(seed);
      std::discrete_distribution<int> fitnessdistribution(fitnesses.begin(), fitnesses.end());
      // Pick two genotypes randomly, until we have a population as big as the initial
      while (amount < genotypes.size()) {
        if (debug) {
          std::cout << "\t\tPopulation size: " << amount << std::endl;
        }
        GenoType inda = genotypes[fitnessdistribution(generator)];
        GenoType indb = genotypes[fitnessdistribution(generator)];
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
        if (uniformdistribution(generator) <= mutateProb) {
          GenoType mutatedGen = genome.mutate(newGen[i]);
          /*
          // Save old genotype for deletion
          GenoType old = newGen.at(i);
          */
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
          outfile << calculateEntropy(newGen) << std::endl;
      }

      // std::move?
      return newGen;
    }

    private:
      int calculateEntropy(std::vector<GenoType> genotypes) {
        unsigned int entropy = 0;
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

#endif
