#include <TFile.h>
#include <TTree.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <vector>


void saveTimeDataToFile(std::ofstream&     outFile,
                        const std::string& inputFileName,
                        int                option) {
	// Open the ROOT file named InputName.root
	std::string rootFileName = inputFileName;
	TFile*      file         = TFile::Open(rootFileName.c_str());

	// Get the tree from the file
	if(!file || file->IsZombie()) {
		std::cerr << "Error: Cannot open file " << rootFileName << std::endl;
		return;
	}

	// Get the tree from the file
	TTree* tree;
	file->GetObject("pulseIERCAnalysis/tree", tree);

	// Check if the tree is valid
	if(!tree) {
		std::cerr << "Error: Tree not found in file " << rootFileName << std::endl;
		file->Close();
		return;
	}

	// Define the variables
	Long64_t                         ewt;
	std::vector<int>*                SiPMID = nullptr;
	std::vector<std::vector<float>>* times  = nullptr;
	map<int, vector<float>>          timeMap;

	// Set the branch addresses
	tree->SetBranchAddress("ewt", &ewt);
	tree->SetBranchAddress("SiPMID", &SiPMID);
	tree->SetBranchAddress("times", &times);

	// Loop over the entries in the tree
	Long64_t nentries = tree->GetEntries();
	std::cout << nentries << " entries" << std::endl;
	for(Long64_t ientry = 0; ientry < nentries; ientry++) {
		if(ientry % 1000 == 0)
			std::cout << "Processing event: " << ientry << std::endl;
		if(!tree->GetEntry(ientry))
			continue;

		if(!times || times->empty()) {
			std::cerr << "Error: 'times' is empty or not loaded correctly!" << std::endl;
		}

		for(int isipm = 0; isipm < SiPMID->size(); isipm++) {
			// Riempio i tempi di questo SiPM
			timeMap[SiPMID->at(isipm)] = times->at(isipm);
		}

		std::ostringstream lineStream;
		lineStream << ewt;

		//std::cout << std::endl;
		// Loop over each sorted SiPM ID and write the data in the required format
		// for (size_t idx : indices) {
		for(auto pair : timeMap) {
			if(option == 1) {
				if(pair.second.size() >= 2) {
					lineStream << "," << pair.second[0] << ","
					           << (pair.second[1] - pair.second[0]);
				} else {
					lineStream << ",NaN,NaN";
				}
			} else if(option == 0) {
				if(!pair.second.empty()) {
					lineStream << "," << pair.second[0];
				} else {
					lineStream << ",NaN";
				}
			}
		}

		// Write the line to the file
		outFile << lineStream.str() << std::endl;
	}

	// Close the file
	file->Close();
}

int calo_gr4_generate_txt(const std::string& inputFileName,
                      const std::string& outputName,
                      int                option) {
	// Create the output file name
	std::string   outputFileName = outputName;
	std::ofstream outFile(outputFileName, std::ios::trunc);
	if(!outFile.is_open()) {
		std::cerr << "Error: Could not open file " << outputFileName << " for writing!"
		          << std::endl;
		return -1;
	}

	gInterpreter->GenerateDictionary("vector<vector<float>>", "vector");

	// Section to insert header
	if(option == 1) {  // Two events
		outFile << "Calo_" + inputFileName +
		               " ewt, ch1_t0, ch1_dt-10us, ch11_t0, ch11_dt-10us, ch20_t0, "
		               "ch20_dt-10us, ch41_t0, ch41_dt-10us, ch51_t0, ch51_dt-10us, "
		               "ch60_t0, ch60_dt-10us"
		        << std::endl;
	} else if(option == 0) {  // One event
		outFile << "Calo_" + inputFileName +
		               " ewt, ch1_t0, ch11_t0, ch20_t0, ch41_t0, ch51_t0, ch60_t0"
		        << std::endl;
	}

	saveTimeDataToFile(outFile, inputFileName, option);
	outFile.close();
	return 0;
}
