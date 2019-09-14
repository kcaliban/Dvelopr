/* Copyright 2019 Fabian Krause */
#include "PoolManager.h"

void PoolMGR::preparePDBQT(std::string ligand) {
  info->infoMsg("(POOLMGR) Preparing PDBQT of ligand: " + ligand);
  // Generate a PDBQT
  std::string command;
  command.append(pythonShPath);
  command.append(" ");
  command.append(mgltoolstilitiesPath);
  command.append("/prepare_ligand4.py -l ");
  command.append(ligand);
  command.append(" -A bonds_hydrogens -U nphs -o ");
  command.append(ligand);
  command.append("qt >/dev/null");
  int success = system(command.c_str());
  if (success != 0) {
    throw VinaException("Could not generate pdbqt file for ligand",
                        ligand,
                        "PQT");
  }
}

std::string PoolMGR::PDBtoFASTA(std::string filename) {
  info->infoMsg("(POOLMGR) Getting FASTA from PDB: " + filename);
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw PoolManagerException(
          "Could not open PDB file to convert to FASTA sequence",
          filename);
  }
  std::unordered_map<std::string, std::string> AA({
    {"ALA", "A"}, {"ARG", "R"}, {"ASN", "N"}, {"ASP", "D"}, {"CYS", "C"},
    {"GLU", "E"}, {"GLN", "Q"}, {"GLY", "G"}, {"HIS", "H"}, {"ILE", "I"},
    {"LEU", "L"}, {"LYS", "K"}, {"MET", "M"}, {"PHE", "F"}, {"PRO", "P"},
    {"SER", "S"}, {"THR", "T"}, {"TRP", "W"}, {"TYR", "Y"}, {"VAL", "V"}
  });

  std::string FASTA;
  std::string line;
  int previd = -1;
  while (getline(file, line)) {
    if (line.substr(0, 4) == "ATOM" || line.substr(0, 6) == "HETATM") {
      std::string aa = line.substr(17, 3);
      int id = stoi(line.substr(22, 4));
      if (id != previd) {
        FASTA.append(AA[aa]);
      }
      previd = id;
    }
  }
  return FASTA;
}

std::string PoolMGR::addElementPDB(std::string file) {
  // Get FASTA sequence
  std::string FASTASEQ = PDBtoFASTA(file);
  // If FASTA is already in pool we can return
  int count;
  #pragma omp critical
  count = internalMap.count(FASTASEQ);
  if (count != 0) { return "";}
  // Make required directory
  std::string command = "mkdir ";
  command.append(workDir);
  command.append("/");
  command.append(FASTASEQ);
  command.append(" 2>/dev/null 1>&2");
  int success = system(command.c_str());
  if (success != 0) {
    PoolManagerException("Could not create directory for PDB file", FASTASEQ);
  }
  command.clear();
  // Move file
  command.append("mv -f ");
  command.append(file);
  command.append(" ");
  command.append(workDir);
  command.append("/");
  command.append(FASTASEQ);
  command.append("/");
  command.append(FASTASEQ);
  command.append(".pdb");
  success = system(command.c_str());
  if (success != 0) {
    throw PoolManagerException("Could not move PDB file", file);
  }
  command.clear();
  // Add file to internal map
  // Generate new vector on the heap
  auto blub = new std::vector<std::pair<std::string, float>>();
  #pragma omp critical
  internalMap[FASTASEQ] = std::make_tuple(FASTASEQ,
                                          workDir + "/" + FASTASEQ + "/" +
                                          FASTASEQ + ".pdb",
                                          *blub, 0);
  // Generate MD and dock
  try {
    info->infoMsg("(POOLMGR) Initiating MD for: " + FASTASEQ);
    genMD(FASTASEQ);
  } catch (GMXException& e) {
    internalMap.erase(internalMap.find(FASTASEQ));  // Remove from map
    throw;  // Throw upwards for handling in main
  }
  // Generate Docking
  try {
    info->infoMsg("(POOLMGR) Initiating Docking for: " + FASTASEQ);
    genDock(FASTASEQ);
  } catch (VinaException& e) {
    internalMap.erase(internalMap.find(FASTASEQ));  // Remove from map

    throw;  // Throw upwards for handling in main
  }
  return FASTASEQ;
}

std::string PoolMGR::toStr() {
  std::string returnStr;
  returnStr.append("[");
  for (auto it = internalMap.begin(); it != internalMap.end(); it++) {
    returnStr.append(it->first);
    returnStr.append(": ");
    for (auto i : std::get<2>(it->second)) {
      returnStr.append(i.first);
      returnStr.append(": ");
      returnStr.append(std::to_string(i.second));
      returnStr.append(", ");
    }
    returnStr.pop_back(); returnStr.pop_back();
    returnStr.append("; ");
  }
  returnStr.pop_back(); returnStr.pop_back();
  returnStr.append("]");

  return returnStr;
}

float PoolMGR::getAffinity(std::string FASTASEQ) {
  float affinity = 100;
  // Return the minimum
  std::vector<std::pair<std::string, float>> vec;
  #pragma omp critical
  vec = std::get<2>(internalMap.at(FASTASEQ));
  for (auto f : vec) {
    if (f.second < affinity) {
      affinity = f.second;
    }
  }
  return affinity;
}

void PoolMGR::addElement(std::string FASTASEQ) {
  int count;
  #pragma omp critical
  count = internalMap.count(FASTASEQ);
  if (count == 0) {
    auto blub = new std::vector<std::pair<std::string, float>>();
    // Add object to map
    #pragma omp critical
    internalMap[FASTASEQ] = std::make_tuple("", "", *blub, 0);
    // Generate PDB, MD and fitness function
    try {
      genPDB(FASTASEQ);
      genMD(FASTASEQ);
      genDock(FASTASEQ);
    } catch (...) {
      throw;
    }
  } else {
    try {
    genMD(FASTASEQ);
    genDock(FASTASEQ);
    } catch (...) {
      throw;
    }
  }
}

void PoolMGR::genPDB(std::string FASTASEQ) {
  // Create directory
  std::string command = "mkdir ";
  command.append(workDir);
  command.append("/");
  command.append(FASTASEQ);
  command.append(" 2>/dev/null 1>&2");
  int success = system(command.c_str());
  if (success != 0) {
    throw PoolManagerException("Could not create directory for PDB file",
                               FASTASEQ);
  }
  command.clear();
  // Run Pymol to create ligand
  command.append(pymolPath);
  command.append(" -kcQ -d \"fab ");
  command.append(FASTASEQ);
  command.append(", ");
  command.append(FASTASEQ);
  command.append(", ss=1;");  // Secondary structure
  command.append("save ");
  command.append(workDir);
  command.append("/");
  command.append(FASTASEQ);
  command.append("/");
  command.append(FASTASEQ);
  command.append(".pdb\"");
  command.append(" >/dev/null 2>&1");
  success = system(command.c_str());
  if (success != 0) {
    throw PoolManagerException("Could not create PDB file", FASTASEQ);
  }
  // Add path to map
  #pragma omp critical
  std::get<0>(internalMap[FASTASEQ]) =
                          workDir + "/" + FASTASEQ + "/" + FASTASEQ + ".pdb";
}

void PoolMGR::genMD(std::string FASTASEQ) {
  std::string prevMD;
  #pragma omp critical
  prevMD = std::get<1>(internalMap[FASTASEQ]);
  std::string mdinput;
  // If there has not been an MD yet, we simply take the original pdb
  if (prevMD == "") {
    #pragma omp critical
    mdinput = std::get<0>(internalMap[FASTASEQ]);
  } else {
  // If there has, use it to do a further MD
    #pragma omp critical
    mdinput = std::get<1>(internalMap[FASTASEQ]);
  }
  GMXInstance gmxInstance(mdinput.c_str(),
                          gromacsPath.c_str(),
                          pymolPath.c_str(),
                          (workDir + "/" + FASTASEQ).c_str(),
                          forcefield.c_str(),
                          forcefieldPath.c_str(),
                          water.c_str(),
                          boundingboxtype.c_str(),
                          clustercutoff,
                          boxsize,
                          mdpPath.c_str(),
                          info);
  try {
    gmxInstance.preparePDB();
    gmxInstance.runMD();
    gmxInstance.clusteredMD();
    gmxInstance.extractTopCluster();
  } catch (...) {
    throw;
  }

  #pragma omp critical
  std::get<1>(internalMap[FASTASEQ]) =
                                workDir + "/" + FASTASEQ + "/topcluster.pdb";
}

void PoolMGR::genDock(std::string FASTASEQ) {
  // Debug: print out all receptors
  /*
  for (auto s : receptors) {
    std::cout << s << std::endl;
  }
  */
  std::vector<std::pair<std::string, float>> affinities;
  // Prepare ligand
  try {
    preparePDBQT(std::get<1>(internalMap[FASTASEQ]));
  } catch (...) {
    throw;
  }
  // Do a docking for each receptor
  #pragma omp parallel
  #pragma omp for
  for (int i = 0; i < nReceptors; i++) {
    VinaInstance vinaInstance(vinaPath.c_str(), receptors.at(i).c_str(),
                              std::get<1>(internalMap[FASTASEQ]).c_str(),
                              info);
    float affinity = vinaInstance.calculateBindingAffinity(exhaustiveness,
                                                           energy_range);
    #pragma omp critical
    affinities.push_back(std::make_pair(receptors.at(i).c_str(), affinity));
  }

  #pragma omp critical
  std::get<2>(internalMap[FASTASEQ]) = affinities;
}

void PoolMGR::update(std::vector<std::string> gen) {
  for (auto FASTASEQ : gen) {
    std::get<3>(internalMap.at(FASTASEQ)) = -1;
  }
  // Increase every number by one
  for (auto it = internalMap.begin(); it != internalMap.end(); it++) {
    std::get<3>(it->second) += 1;
  }
}

void PoolMGR::cleanUp(int x) {
  // Cleans all files of PDBs that have not been used for x generations
  for (auto it = internalMap.begin(); it != internalMap.end(); ) {
    if (std::get<3>(it->second) > x) {
      deleteElementData(it->first);
      free(&std::get<2>(it->second));  // Free up heap space
      internalMap.erase(it++);
    } else {
      it++;
    }
  }
}

void PoolMGR::deleteElementData(std::string FASTASEQ) {
  std::cout << "Has not been used for some generations: "
            << FASTASEQ << std::endl;
  // Simply remove the directory recursively
  std::string command;
  command.append("rm -rf ");
  command.append(workDir);
  command.append("/");
  command.append(FASTASEQ);
  int success = system(command.c_str());
  if (success != 0) {
    throw PoolManagerException("Could not clean unused PDB files",
              workDir + "/" + FASTASEQ);
  }
}