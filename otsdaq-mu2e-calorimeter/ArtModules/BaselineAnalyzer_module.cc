#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "TRACE/tracemf.h"
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME "BaselineAnalyzer"

#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"

#include <artdaq-core/Data/ContainerFragment.hh>
#include "artdaq-core/Data/Fragment.hh"

#include "Offline/DAQ/inc/CaloDAQUtilities.hh"
#include "Offline/DataProducts/inc/CaloConst.hh"
#include "Offline/RecoDataProducts/inc/CaloDigi.hh"
#include "artdaq-core-mu2e/Data/EventHeader.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/Decoders/CalorimeterDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

#include "cetlib_except/exception.h"

//-- insert calls to proditions ..for calodmap-----
#include "Offline/CaloConditions/inc/CaloDAQMap.hh"
#include "Offline/ProditionsService/inc/ProditionsHandle.hh"
//-------------------------------------------------

#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include <sys/stat.h>
#include "TF1.h"
#include "TFile.h"
#include "TFitResult.h"
#include "TFitResultPtr.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TH1.h"
#include "TH2.h"
#include "TROOT.h"
#include "TStyle.h"
#include "TTree.h"
#include "art_root_io/TFileService.h"

#include "Offline/CaloVisualizer/inc/THMu2eCaloDisk.hh"

namespace mu2e {
class BaselineAnalyzer : public art::EDAnalyzer {
  public:
	// clang-format off
    struct Config {
      fhicl::Atom<std::string> caloDigiTag {fhicl::Name("caloDigiTag" ) , fhicl::Comment("caloDigiTag"), ""};
      fhicl::Atom<int> verbosity {fhicl::Name("verbosity" ) , fhicl::Comment("Verbosity [0-2]"), 0};
      fhicl::Atom<bool> writeThresholds {fhicl::Name("writeThresholds" ) , fhicl::Comment("Write text files with thresholds"), false};
      fhicl::Atom<bool> overwriteThresholds {fhicl::Name("overwriteThresholds" ) , fhicl::Comment("Overwrite existing files"), false};
      fhicl::Atom<std::string> thresholdFolder {fhicl::Name("thresholdFolder" ) , fhicl::Comment("Folder to write thresholds into"), ""};
      fhicl::Atom<int> thresholdOffset {fhicl::Name("thresholdOffset" ) , fhicl::Comment("Offset with respect to gaussian mean"), 100};
      fhicl::Atom<int> thresholdOffsetPin {fhicl::Name("thresholdOffsetPin" ) , fhicl::Comment("Offset with respect to gaussian mean (pin diodes)"), 50};
      fhicl::Atom<int> hotStdDev {fhicl::Name("hotStdDev" ) , fhicl::Comment("StdDev limit for hot channels"), 6};
      fhicl::Atom<int> coldStdDev {fhicl::Name("coldStdDev" ) , fhicl::Comment("StdDev limit for cold channels"), 3};
    };
	// clang-format on

	explicit BaselineAnalyzer(const art::EDAnalyzer::Table<Config>& config);
	void analyze(art::Event const& event) override;
	void endJob() override;
	void FitHistograms();

  private:
	std::string caloDigiTag_;
	int         verbosity_;
	bool        writeThresholds_;
	bool        overwriteThresholds_;
	std::string thresholdFolder_;
	int         thresholdOffset_;
	int         thresholdOffsetPin_;
	double      hotStdDev_;
	double      coldStdDev_;

	mu2e::ProditionsHandle<mu2e::CaloDAQMap> _calodaqconds_h;

	double xmin;
	double xmax;

	std::map<int, std::map<int, mu2e::CaloConst::detType>> channelType;
	std::map<int, std::map<int, TH1D*>>                    h1_baseline_map;

	TH2D*                 h2_baselines;
	TH1D*                 h1_means;
	TH1D*                 h1_sigmas;
	TH1D*                 h1_threshold;
	TGraphErrors*         g_baselines;
	TGraph*               g_fitsigmas;
	TGraph*               g_stddev;
	TGraph*               g_stddev_pin;
	TGraph*               g_stddev_lyso;
	TGraph*               g_stddev_empty;
	TGraph*               g_thresholds;
	mu2e::THMu2eCaloDisk* h2_disk0;
	mu2e::THMu2eCaloDisk* h2_disk1;
};
}  // namespace mu2e

mu2e::BaselineAnalyzer::BaselineAnalyzer(const art::EDAnalyzer::Table<Config>& config)
    : art::EDAnalyzer{config}
    , caloDigiTag_(config().caloDigiTag())
    , verbosity_(config().verbosity())
    , writeThresholds_(config().writeThresholds())
    , overwriteThresholds_(config().overwriteThresholds())
    , thresholdFolder_(config().thresholdFolder())
    , thresholdOffset_(config().thresholdOffset())
    , thresholdOffsetPin_(config().thresholdOffsetPin())
    , hotStdDev_(config().hotStdDev())
    , coldStdDev_(config().coldStdDev()) {
	art::ServiceHandle<art::TFileService> tfs;

	xmin         = 2048 - 150;
	xmax         = 2048 + 300;
	h2_baselines = tfs->make<TH2D>("h2_baselines", "Baselines;BoardID*100 + ChannelID;Baseline [ADC]", 20000, 0, 20000, xmax - xmin, xmin, xmax);
	h1_means     = tfs->make<TH1D>("h1_means", "All channels baselines;ADC", int(0.5 * (xmax - xmin)), xmin, xmax);
	h1_sigmas    = tfs->make<TH1D>("h1_sigmas", "All channels baseline sigmas;ADC", 200, 0, 50);
	h1_threshold = tfs->make<TH1D>("h1_threshold", "All channel thresholds;ADC", int(0.5 * (xmax - xmin)), xmin, xmax);
	g_baselines  = tfs->makeAndRegister<TGraphErrors>("g_baselines", "Baselines;BoardID*100 + ChannelID;Baseline [ADC]");
	g_baselines->SetMarkerStyle(20);
	g_fitsigmas = tfs->makeAndRegister<TGraph>("g_fitsigmas", "Gaussian fit sigmas;BoardID*100 + ChannelID;ADC");
	g_fitsigmas->SetMarkerStyle(20);
	g_stddev = tfs->makeAndRegister<TGraph>("g_stddev", "Channel StdDev;BoardID*100 + ChannelID;ADC");
	g_stddev->SetMarkerStyle(20);
	g_stddev_pin = tfs->makeAndRegister<TGraph>("g_stddev_pin", "[pin-diode];BoardID*100 + ChannelID;ADC");
	g_stddev_pin->SetMarkerStyle(20);
	g_stddev_pin->SetMarkerColor(kRed);
	g_stddev_lyso = tfs->makeAndRegister<TGraph>("g_stddev_lyso", "[LYSO];BoardID*100 + ChannelID;ADC");
	g_stddev_lyso->SetMarkerStyle(20);
	g_stddev_lyso->SetMarkerColor(kGreen);
	g_stddev_empty = tfs->makeAndRegister<TGraph>("g_stddev_empty", "[empty];BoardID*100 + ChannelID;ADC");
	g_stddev_empty->SetMarkerStyle(20);
	g_stddev_empty->SetMarkerColor(kGray);
	g_thresholds = tfs->makeAndRegister<TGraph>("g_thresholds", "Thresholds;BoardID*100 + ChannelID;ADC");
	g_thresholds->SetMarkerStyle(20);
	g_thresholds->SetMarkerColor(kRed);
	h2_disk0 = tfs->makeAndRegister<mu2e::THMu2eCaloDisk>("h2_disk0", "Disk 0", "h2_disk0", "Disk 0", 0);
	h2_disk1 = tfs->makeAndRegister<mu2e::THMu2eCaloDisk>("h2_disk1", "Disk 1", "h2_disk1", "Disk 1", 1);
}

void mu2e::BaselineAnalyzer::analyze(art::Event const& event) {
	const auto& caloDigis = *event.getValidHandle(consumes<mu2e::CaloDigiCollection>(caloDigiTag_));

	// Loop over the calo digis of this event
	for(uint ihit = 0; ihit < caloDigis.size(); ihit++) {
		int              SiPMID   = caloDigis[ihit].SiPMID();
		std::vector<int> waveform = caloDigis[ihit].waveform();

		mu2e::CaloDAQMap const& calodaqconds = _calodaqconds_h.get(event.id());
		mu2e::CaloSiPMId        SiPMID_(SiPMID);
		int                     BoardChan = calodaqconds.rawId(SiPMID_).id();
		int                     boardID   = BoardChan / 20;
		int                     chanID    = BoardChan % 20;

		// If new channel, create hist
		if(h1_baseline_map[boardID].find(chanID) == h1_baseline_map[boardID].end()) {
			channelType[boardID][chanID]     = SiPMID_.detType();
			bool    pindiode                 = (SiPMID_.detType() == mu2e::CaloConst::detType::PINDiode);
			TString hname                    = Form("h1_baseline_b%03d_c%02d", boardID, chanID);
			TString htitle                   = Form("Baseline of Board %03d channel %02d%s", boardID, chanID, (pindiode ? " [PIN-DIODE]" : ""));
			h1_baseline_map[boardID][chanID] = new TH1D(hname, htitle, xmax - xmin, xmin, xmax);
		}

		// Fill hist
		for(auto sample : waveform) {
			h2_baselines->Fill(boardID * 100 + chanID, sample);
			h1_baseline_map[boardID][chanID]->Fill(sample);
		}
	}
}

void mu2e::BaselineAnalyzer::endJob() {
	FitHistograms();
	// art::ServiceHandle<art::TFileService> tfs;
	// tfs->file().WriteTObject(h2_disk0, "h2_disk0");
	// tfs->file().WriteTObject(h2_disk1, "h2_disk1");
}

void mu2e::BaselineAnalyzer::FitHistograms() {
	// Perform gaussian fits
	int                           failed_fits    = 0;
	int                           unprecise_fits = 0;
	std::set<std::pair<int, int>> failed_map;
	std::set<std::pair<int, int>> unprecise_map;
	for(auto board_pair : h1_baseline_map) {
		int           board = board_pair.first;
		std::ofstream outputBaselineFile;
		TString       fname         = Form("%s/dirac%03d.baseline", thresholdFolder_.c_str(), board);
		bool          writeThisFile = writeThresholds_;
		if(writeThisFile && !overwriteThresholds_) {
			// Check if the file already exists
			struct stat buffer;
			if(stat(fname.Data(), &buffer) != -1) {
				writeThisFile = false;
			}
		}

		if(writeThisFile) {
			outputBaselineFile.open(fname);
			if(!outputBaselineFile.is_open()) {
				std::cout << "Warning! Can't open file " << fname << "\n";
				writeThisFile = false;
			}
		}

		for(auto channel_pair : board_pair.second) {
			int channel = channel_pair.first;

			bool pindiode = (channelType[board][channel] == mu2e::CaloConst::detType::PINDiode);

			h1_baseline_map[board][channel]->Scale(1. / h1_baseline_map[board][channel]->Integral());

			double stddev = h1_baseline_map[board][channel]->GetRMS();
			if(stddev > hotStdDev_) {
				std::cout << "Hot channel: Board " << board << " Channel " << channel << " (" << std::setprecision(4) << std::setw(5) << stddev << ") [" << channelType[board][channel] << "]\n";
			}
			if(stddev < coldStdDev_) {
				std::cout << "Cold channel: Board " << board << " Channel " << channel << " (" << std::setprecision(4) << std::setw(5) << stddev << ") [" << channelType[board][channel] << "]\n";
			}

			switch(channelType[board][channel]) {
			case mu2e::CaloConst::detType::CsI:
				g_stddev->AddPoint(board * 100 + channel, stddev);
				break;
			case mu2e::CaloConst::detType::PINDiode:
				g_stddev_pin->AddPoint(board * 100 + channel, stddev);
				break;
			case mu2e::CaloConst::detType::CAPHRI:
				g_stddev_lyso->AddPoint(board * 100 + channel, stddev);
				break;
			case mu2e::CaloConst::detType::Invalid:
				g_stddev_empty->AddPoint(board * 100 + channel, stddev);
				break;
			}

			/*
			if (maptype[board][channel]=="CAL"){
			  g_stddev->AddPoint(board*100+channel,stddev);
			} else if (maptype[board][channel]=="PIN-DIODE"){
			  g_stddev_pin->AddPoint(board*100+channel,stddev);
			} else if (maptype[board][channel]=="CAPHRI"){
			  g_stddev_lyso->AddPoint(board*100+channel,stddev);
			} else if (maptype[board][channel]=="EMPTY"){
			  g_stddev_empty->AddPoint(board*100+channel,stddev);
			} else {
			  cout<<"Board "<<board<<" Channel "<<channel<<" : unknown type "<<maptype[board][channel]<<"\n";
			}
			*/

			std::cout << "Fitting board " << board << " channel " << channel << " ... ";
			double guess_max   = h1_baseline_map[board][channel]->GetBinContent(h1_baseline_map[board][channel]->GetMaximumBin());
			double guess_mean  = h1_baseline_map[board][channel]->GetMean();
			double guess_sigma = h1_baseline_map[board][channel]->GetRMS();

			((TF1*)gROOT->GetFunction("gaus"))->SetParameters(guess_max, guess_mean, guess_sigma);
			TFitResultPtr fit_result = h1_baseline_map[board][channel]->Fit("gaus", "SQW", "", xmin, xmax);
			h1_baseline_map[board][channel]->GetFunction("gaus")->SetNpx(1000);
			if(fit_result < 0) {
				std::cout << " <---- BAD FIT!!! Status " << fit_result << "\n";
				failed_fits++;
				failed_map.insert(std::make_pair(board, channel));
				continue;
			}
			double fit_mean  = fit_result->Parameter(1);
			double fit_sigma = fit_result->Parameter(2);
			double fit_chi2  = fit_result->Chi2();
			double fit_ndf   = fit_result->Ndf();
			int    threshold = int(round(fit_mean + thresholdOffset_));

			// Pin diodes
			if(pindiode) {
				threshold = int(round(fit_mean + thresholdOffsetPin_));
			}

			std::cout << fit_mean << " +- " << fit_sigma << " chi2/ndf=" << fit_chi2 << "/" << fit_ndf;
			if(fit_result > 0) {
				std::cout << " <---- UNPRECISE! Status " << fit_result;
				unprecise_fits++;
				unprecise_map.insert(std::make_pair(board, channel));
			}
			std::cout << "\n";

			h1_means->Fill(fit_mean);
			h1_sigmas->Fill(fit_sigma);
			h1_threshold->Fill(threshold);
			g_baselines->SetPoint(g_baselines->GetN(), board * 100 + channel, fit_mean);
			g_baselines->SetPointError(g_baselines->GetN() - 1, 0, fit_sigma);
			g_fitsigmas->SetPoint(g_fitsigmas->GetN(), board * 100 + channel, fit_sigma);
			g_thresholds->SetPoint(g_thresholds->GetN(), board * 100 + channel, threshold);
			if(board < 80) {
				h2_disk0->FillRaw(board, channel, board);  // stddev);
			} else {
				h2_disk1->FillRaw(board, channel, board);  // stddev);
			}

			TString lineout = Form("%d\t%.2f\t%.2f\t%d", channel, fit_mean, fit_sigma, threshold);
			if(writeThisFile) {
				outputBaselineFile << lineout << "\n";
			}
		}
		if(writeThisFile) {
			outputBaselineFile.close();
		}
	}

	std::cout << "Failed fits: " << failed_fits << "\n";
	for(auto pair : failed_map) {
		std::cout << "Board " << pair.first << " Channel " << pair.second << "\n";
	}

	std::cout << "Unprecise fits: " << unprecise_fits << "\n";
	for(auto pair : unprecise_map) {
		std::cout << "Board " << pair.first << " Channel " << pair.second << "\n";
	}

	std::cout << "Disk 0: " << h2_disk0->GetEntries() << " entries\n";
	std::cout << "Disk 1: " << h2_disk1->GetEntries() << " entries\n";
}

DEFINE_ART_MODULE(mu2e::BaselineAnalyzer)
