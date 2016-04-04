#include "include/ROC/ROCParameter.h"

#ifndef STANDALONE
#include <Rcpp.h>
using namespace Rcpp;
#endif

//--------------------------------------------------//
// ---------- Constructors & Destructors ---------- //
//--------------------------------------------------//


ROCParameter::ROCParameter() : Parameter()
{
	//CTOR
	bias_csp = 0;
	mutation_prior_sd = 0.35;
	currentCodonSpecificParameter.resize(2);
	proposedCodonSpecificParameter.resize(2);
}


ROCParameter::ROCParameter(std::string filename) : Parameter(22)
{
	currentCodonSpecificParameter.resize(2);
	proposedCodonSpecificParameter.resize(2);
	initFromRestartFile(filename);
}


ROCParameter::ROCParameter(std::vector<double> stdDevSynthesisRate, unsigned _numMixtures, std::vector<unsigned> geneAssignment,
		std::vector<std::vector<unsigned>> thetaKMatrix, bool splitSer, std::string _mutationSelectionState) :
		Parameter(22)
{
	initParameterSet(stdDevSynthesisRate, _numMixtures, geneAssignment, thetaKMatrix, splitSer, _mutationSelectionState);
	initROCParameterSet();
}


ROCParameter& ROCParameter::operator=(const ROCParameter& rhs)
{
	if (this == &rhs)
		return *this; // handle self assignment

	Parameter::operator=(rhs);

	covarianceMatrix = rhs.covarianceMatrix;
	// proposal bias and std for codon specific parameter
	bias_csp = rhs.bias_csp;
	std_csp = rhs.std_csp;

	noiseOffset = rhs.noiseOffset;
	noiseOffset_proposed = rhs.noiseOffset_proposed;
	std_NoiseOffset = rhs.std_NoiseOffset;
	numAcceptForNoiseOffset = rhs.numAcceptForNoiseOffset;

	return *this;
}


ROCParameter::~ROCParameter()
{
	//DTOR
}





//---------------------------------------------------------------//
// ---------- Initialization, Restart, Index Checking ---------- //
//---------------------------------------------------------------//


void ROCParameter::initROCParameterSet()
{
	mutation_prior_sd = 0.35;

	groupList = {"A", "C", "D", "E", "F", "G", "H", "I", "K", "L", "N", "P", "Q", "R", "S", "T", "V", "Y", "Z"};
	// proposal bias and std for codon specific parameter
	bias_csp = 0;
	

	for (unsigned i = 0; i < getNumObservedPhiSets(); i++) {
		noiseOffset[i] = 0.1;
		noiseOffset_proposed[i] = 0.1;
		std_NoiseOffset[i] = 0.1;
		observedSynthesisNoise[i] = 0.1;
		numAcceptForNoiseOffset[i] = 0;
	}

	//may need getter fcts

	proposedCodonSpecificParameter.resize(2);
	currentCodonSpecificParameter.resize(2);

	currentCodonSpecificParameter[dM].resize(numMutationCategories);
	proposedCodonSpecificParameter[dM].resize(numMutationCategories);

	for (unsigned i = 0u; i < numMutationCategories; i++)
	{
		std::vector<double> tmp(numParam, 0.0);
		currentCodonSpecificParameter[dM][i] = tmp;
		proposedCodonSpecificParameter[dM][i] = tmp;
	}

	currentCodonSpecificParameter[dEta].resize(numSelectionCategories);
	proposedCodonSpecificParameter[dEta].resize(numSelectionCategories);

	for (unsigned i = 0u; i < numSelectionCategories; i++)
	{
		std::vector<double> tmp(numParam, 0.0);
		proposedCodonSpecificParameter[dEta][i] = tmp;
		currentCodonSpecificParameter[dEta][i] = tmp;
	}

  for (unsigned i = 0; i < maxGrouping; i++)
  {
    std::string aa = SequenceSummary::AminoAcidArray[i];
    unsigned numCodons = SequenceSummary::GetNumCodonsForAA(aa, true);
    CovarianceMatrix m((numMutationCategories + numSelectionCategories) * numCodons);
    m.choleskiDecomposition();
    covarianceMatrix.push_back(m);
  }
}


void ROCParameter::initROCValuesFromFile(std::string filename)
{
	std::ifstream input;
	covarianceMatrix.resize(maxGrouping);
	std::vector <double> mat;
	input.open(filename.c_str());
	if (input.fail())
	{
#ifndef STANDALONE
		Rf_error("Error opening file %s to initialize from restart file.\n", filename.c_str());
#else
		std::cerr << "Error opening file " << filename << " to initialize from restart file.\n";
#endif
	}
	else
	{
		std::string tmp, variableName;
		unsigned cat = 0;
		while (getline(input, tmp))
		{
			int flag;
			if (tmp[0] == '>')
				flag = 1;
			else if (input.eof() || tmp == "\n")
				flag = 2;
			else if (tmp[0] == '#')
				flag = 3;
			else
				flag = 4;

			if (flag == 1)
			{
				mat.clear();
				cat = 0;
				variableName = tmp.substr(1,tmp.size()-2);
				if (variableName == "covarianceMatrix")
				{
					getline(input,tmp);
					//char aa = tmp[0];
					cat = SequenceSummary::AAToAAIndex(tmp); // ????
				}
			}
			else if (flag == 2)
			{
#ifndef STANDALONE
				Rprintf("here\n");
#else
				std::cout << "here\n";
#endif
			}
			else if (flag == 3) //user comment, continue
			{
				continue;
			}
			else
			{
				std::istringstream iss;
				if (variableName == "currentMutationParameter")
				{
					if (tmp == "***")
					{
						currentCodonSpecificParameter[dM].resize(currentCodonSpecificParameter[dM].size() + 1);
						cat++;
					}
					else if (tmp == "\n")
						continue;
					else
					{
						double val;
						iss.str(tmp);
						while (iss >> val)
						{
							currentCodonSpecificParameter[dM][cat - 1].push_back(val);
						}
					}
				}
				else if (variableName == "currentSelectionParameter")
				{
					if (tmp == "***")
					{
						currentCodonSpecificParameter[dEta].resize(currentCodonSpecificParameter[dEta].size() + 1);
						cat++;
					}
					else if (tmp == "\n")
						continue;
					else
					{
						double val;
						iss.str(tmp);
						while (iss >> val)
						{
							currentCodonSpecificParameter[dEta][cat - 1].push_back(val);
						}
					}
				}
				else if (variableName == "std_csp")
				{
					double val;
					iss.str(tmp);
					while (iss >> val) {
						std_csp.push_back(val);
					}
				}
				else if (variableName == "noiseOffset")
				{
					double val;
					iss.str(tmp);
					while (iss >> val) {
						noiseOffset.push_back(val);
					}
				}
				else if (variableName == "observedSynthesisNoise")
				{
					double val;
					iss.str(tmp);
					while (iss >> val) {
						observedSynthesisNoise.push_back(val);
					}
				}
				else if (variableName == "std_NoiseOffset")
				{
					double val;
					iss.str(tmp);
					while (iss >> val) {
						std_NoiseOffset.push_back(val);
					}
				}
				else if (variableName == "covarianceMatrix")
				{
					if (tmp == "***") //end of matrix
					{
						CovarianceMatrix CM(mat);
						covarianceMatrix[cat] = CM;
					}
					double val;
					iss.str(tmp);
					while (iss >> val)
					{
						mat.push_back(val);
					}
				}
				else if (variableName == "mutation_prior_sd")
				{
					iss.str(tmp);
					iss >> mutation_prior_sd;
				}
			}
		}
	}
	input.close();

	//init other values
	bias_csp = 0;
	proposedCodonSpecificParameter[dM].resize(numMutationCategories);
	proposedCodonSpecificParameter[dEta].resize(numSelectionCategories);
	for (unsigned i = 0; i < numMutationCategories; i++)
	{
		proposedCodonSpecificParameter[dM][i] = currentCodonSpecificParameter[dM][i];
	}
	for (unsigned i = 0; i < numSelectionCategories; i++)
	{
		proposedCodonSpecificParameter[dEta][i] = currentCodonSpecificParameter[dEta][i];
	}
}


void ROCParameter::writeEntireRestartFile(std::string filename)
{
	writeBasicRestartFile(filename);
	writeROCRestartFile(filename);
}


void ROCParameter::writeROCRestartFile(std::string filename)
{
	std::ofstream out;
	out.open(filename.c_str(), std::ofstream::app);
	if (out.fail())
	{
#ifndef STANDALONE
		Rf_error("Error opening file %s to write restart file.\n", filename.c_str());
#else
		std::cerr << "Error opening file " << filename << " to write restart file.\n";
#endif
	}
	else
	{
		std::ostringstream oss;
		unsigned j;
		oss << ">noiseOffset:\n";
		for (j = 0; j < noiseOffset.size(); j++)
		{
			oss << noiseOffset[j];
			if ((j + 1) % 10 == 0)
				oss << "\n";
			else
				oss << " ";
		}
		if (j % 10 != 0)
			oss << "\n";

		oss << ">observedSynthesisNoise:\n";
		for (j = 0; j < observedSynthesisNoise.size(); j++)
		{
			oss << observedSynthesisNoise[j];
			if ((j + 1) % 10 == 0)
				oss << "\n";
			else
				oss << " ";
		}
		if (j % 10 != 0)
			oss << "\n";

		oss << ">mutation_prior_sd:\n" << mutation_prior_sd << "\n";
		oss << ">std_NoiseOffset:\n";
		for (j = 0; j < std_NoiseOffset.size(); j++)
		{
			oss << std_NoiseOffset[j];
			if ((j + 1) % 10 == 0)
				oss << "\n";
			else
				oss << " ";
		}
		if (j % 10 != 0)
			oss << "\n";
		oss << ">std_csp:\n";
		for (j = 0; j < std_csp.size(); j++)
		{
			oss << std_csp[j];
			if ((j + 1) % 10 == 0)
				oss << "\n";
			else
				oss << " ";
		}
		if (j % 10 != 0)
			oss << "\n";
		oss << ">currentMutationParameter:\n";
		for (unsigned i = 0; i < currentCodonSpecificParameter[dM].size(); i++)
		{
			oss << "***\n";
			for (j = 0; j < currentCodonSpecificParameter[dM][i].size(); j++)
			{
				oss << currentCodonSpecificParameter[dM][i][j];
				if ((j + 1) % 10 == 0)
					oss << "\n";
				else
					oss << " ";
			}
			if (j % 10 != 0)
				oss << "\n";
		}

		oss << ">currentSelectionParameter:\n";
		for (unsigned i = 0; i < currentCodonSpecificParameter[dEta].size(); i++)
		{
			oss << "***\n";
			for (j = 0; j < currentCodonSpecificParameter[dEta][i].size(); j++)
			{
				oss << currentCodonSpecificParameter[dEta][i][j];
				if ((j + 1) % 10 == 0)
					oss << "\n";
				else
					oss << " ";
			}
			if (j % 10 != 0)
				oss << "\n";
		}

		for (unsigned i = 0; i < groupList.size(); i++)
		{
			std::string aa = groupList[i];
			oss << ">covarianceMatrix:\n" << aa << "\n";
			CovarianceMatrix m = covarianceMatrix[SequenceSummary::AAToAAIndex(aa)];
			std::vector<double>* tmp = m.getCovMatrix();
			int size = m.getNumVariates();
			for(unsigned k = 0; k < size * size; k++)
			{
				if (k % size == 0 && k != 0) { oss << "\n"; }
				oss << tmp->at(k) << "\t";
			}
			oss << "\n***\n";
		}
		std::string output = oss.str();
		out << output;
	}
	out.close();
}


void ROCParameter::initFromRestartFile(std::string filename)
{
	initBaseValuesFromFile(filename);
	initROCValuesFromFile(filename);
}


void ROCParameter::initAllTraces(unsigned samples, unsigned num_genes)
{
	traces.initializeROCTrace(samples, num_genes, numMutationCategories, numSelectionCategories, numParam,
						 numMixtures, categories, maxGrouping, obsPhiSets);
}


void ROCParameter::initMutationCategories(std::vector<std::string> files, unsigned numCategories)
{
	for (unsigned category = 0; category < numCategories; category++)
	{
		//Open the file for the category
		std::ifstream currentFile;
		currentFile.open(files[category].c_str());
		if (currentFile.fail())
		{
#ifndef STANDALONE
			Rf_error("Error opening file %d to initialize mutation values.\n", category);
#else
			std::cerr << "Error opening file " << category << " to initialize mutation values.\n";
#endif
		}
		else
		{
			std::string tmp;
			currentFile >> tmp; //The first line is a header (Amino Acid, Codon, Value, Std_deviation)

			while (currentFile >> tmp)
			{
				//Get the Codon and Index
				std::size_t pos = tmp.find(",", 2); //Amino Acid and a comma will always be the first 2 characters
				std::string codon = tmp.substr(2, pos - 2);
				unsigned codonIndex = SequenceSummary::codonToIndex(codon, true);

				//get the value to store
				std::size_t pos2 = tmp.find(",", pos + 1);
				//std::cout << tmp.substr(pos + 1, pos2 - pos - 1 ) <<"\n";
				double value = std::atof(tmp.substr(pos + 1, pos2 - pos - 1).c_str());

				currentCodonSpecificParameter[dM][category][codonIndex] = value;
				proposedCodonSpecificParameter[dM][category][codonIndex] = value;
			}
		}

		currentFile.close();
	} //END OF A CATEGORY/FILE
}


void ROCParameter::initSelectionCategories(std::vector<std::string> files, unsigned numCategories)
{
	for (unsigned category = 0; category < numCategories; category++)
	{
		//Open the file for the category
		std::ifstream currentFile;
		currentFile.open(files[category].c_str());
		if (currentFile.fail())
		{
#ifndef STANDALONE
			Rf_error("Error opening file %d to initialize mutation values.\n", category);
#else
			std::cerr << "Error opening file " << category << " to initialize mutation values.\n";
#endif
		}
		else
		{
			std::string tmp;
			currentFile >> tmp; //The first line is a header (Amino Acid, Codon, Value, Std_deviation)

			while (currentFile >> tmp)
			{
				//Get the Codon and Index
				std::size_t pos = tmp.find(",", 2); //Amino Acid and a comma will always be the first 2 characters
				std::string codon = tmp.substr(2, pos - 2);
				unsigned codonIndex = SequenceSummary::codonToIndex(codon, true);

				//get the value to store
				std::size_t pos2 = tmp.find(",", pos + 1);
				//	std::cout << tmp.substr(pos + 1, pos2 - pos - 1 ) <<"\n";
				double value = std::atof(tmp.substr(pos + 1, pos2 - pos - 1).c_str());

				currentCodonSpecificParameter[dEta][category][codonIndex] = value;
				proposedCodonSpecificParameter[dEta][category][codonIndex] = value;
			}
		}

		currentFile.close();
	} //END OF A CATEGORY/FILE
}


// --------------------------------------//
// ---------- Trace Functions -----------//
// --------------------------------------//

void ROCParameter::updateObservedSynthesisNoiseTraces(unsigned sample)
{
	for (unsigned i = 0; i < observedSynthesisNoise.size(); i++)
	{
		traces.updateObservedSynthesisNoiseTrace(i, sample, observedSynthesisNoise[i]);
	}
}


void ROCParameter::updateNoiseOffsetTraces(unsigned sample)
{
	for (unsigned i = 0; i < noiseOffset.size(); i++)
	{
		traces.updateSynthesisOffsetTrace(i, sample, noiseOffset[i]);
	}
}

void ROCParameter::updateCodonSpecificParameterTrace(unsigned sample, std::string grouping)
{
	traces.updateCodonSpecificParameterTraceForAA(sample, grouping, currentCodonSpecificParameter[dM], dM);
	traces.updateCodonSpecificParameterTraceForAA(sample, grouping, currentCodonSpecificParameter[dEta], dEta);
}



// ------------------------------------------//
// ---------- Covariance Functions ----------//
// ------------------------------------------//


CovarianceMatrix& ROCParameter::getCovarianceMatrixForAA(std::string aa)
{
	aa[0] = (char) std::toupper(aa[0]);
	unsigned aaIndex = SequenceSummary::aaToIndex.find(aa) -> second;
	return covarianceMatrix[aaIndex];
}


// ------------------------------------------------------//
// ---------- observedSynthesisNoise Functions ----------//
// ------------------------------------------------------//


double ROCParameter::getObservedSynthesisNoise(unsigned index)
{
	return observedSynthesisNoise[index];
}


void ROCParameter::setObservedSynthesisNoise(unsigned index, double se)
{
	observedSynthesisNoise[index] = se;
}





// -------------------------------------------//
// ---------- noiseOffset Functions ----------//
// -------------------------------------------//


double ROCParameter::getNoiseOffset(unsigned index, bool proposed)
{
	return (proposed ? noiseOffset_proposed[index] : noiseOffset[index]);
}


double ROCParameter::getCurrentNoiseOffsetProposalWidth(unsigned index)
{
	return std_NoiseOffset[index];
}


void ROCParameter::proposeNoiseOffset()
{
	for (unsigned i = 0; i < getNumObservedPhiSets(); i++) {
		noiseOffset_proposed[i] = randNorm(noiseOffset[i], std_NoiseOffset[i]);
	}
}


void ROCParameter::setNoiseOffset(unsigned index, double _noiseOffset)
{
	noiseOffset[index] = _noiseOffset;
}


void ROCParameter::updateNoiseOffset(unsigned index)
{
	noiseOffset[index] = noiseOffset_proposed[index];
	numAcceptForNoiseOffset[index]++;
}


// -----------------------------------//
// ---------- CSP Functions ----------//
// -----------------------------------//


double ROCParameter::getCurrentCodonSpecificProposalWidth(unsigned aa)
{
	unsigned aaStart;
	unsigned aaEnd;
	SequenceSummary::AAIndexToCodonRange(aa, aaStart, aaEnd, true);
	return std_csp[aaStart];
}


// Cedric: I decided to use a normal distribution to propose Sphi and phi instead of a lognormal because:
// 1. It is a symmetric distribution and you therefore do not have to account for the unsymmetry in jump probabilities
// 2. The one log and exp operation that have to be performed per parameter are cheaper than the operations necessary to draw from a lognormal
// 3. phi has to be on a non log scale for the likelihood evaluation thus it does not help to keep phi on th elog scale all the time
// 4. the adjusment of the likelihood by the jacobian that arises from this transformation is cheap and by grouping everything in one class it takes place more or less at the same place
void ROCParameter::proposeCodonSpecificParameter()
{

	for (unsigned k = 0; k < getGroupListSize(); k++)
	{
		std::vector<double> iidProposed;
		std::string aa = getGrouping(k);
		unsigned aaStart;
		unsigned aaEnd;
		SequenceSummary::AAToCodonRange(aa, aaStart, aaEnd, true);
		unsigned numCodons = aaEnd - aaStart;
		for (unsigned i = 0u; i < (numCodons * (numMutationCategories + numSelectionCategories)); i++)
		{
			iidProposed.push_back(randNorm(0.0, 1.0));
		}

		std::vector<double> covaryingNums;
		covaryingNums = covarianceMatrix[SequenceSummary::AAToAAIndex(aa)].transformIidNumersIntoCovaryingNumbers(
				iidProposed);
		for (unsigned i = 0; i < numMutationCategories; i++)
		{
			for (unsigned j = i * numCodons, l = aaStart; j < (i * numCodons) + numCodons; j++, l++)
			{
				proposedCodonSpecificParameter[dM][i][l] = currentCodonSpecificParameter[dM][i][l] + covaryingNums[j];
			}
		}
		for (unsigned i = 0; i < numSelectionCategories; i++)
		{
			for (unsigned j = i * numCodons, l = aaStart; j < (i * numCodons) + numCodons; j++, l++)
			{
				proposedCodonSpecificParameter[dEta][i][l] = currentCodonSpecificParameter[dEta][i][l]
												   + covaryingNums[(numMutationCategories * numCodons) + j];
			}
		}
	}
}


void ROCParameter::updateCodonSpecificParameter(std::string grouping)
{
	unsigned aaStart;
	unsigned aaEnd;
	SequenceSummary::AAToCodonRange(grouping, aaStart, aaEnd, true);
	unsigned aaIndex = SequenceSummary::aaToIndex.find(grouping)->second;
	numAcceptForCodonSpecificParameters[aaIndex]++;

	for (unsigned k = 0u; k < numMutationCategories; k++)
	{
		for (unsigned i = aaStart; i < aaEnd; i++)
		{
			currentCodonSpecificParameter[dM][k][i] = proposedCodonSpecificParameter[dM][k][i];
		}
	}
	for (unsigned k = 0u; k < numSelectionCategories; k++)
	{
		for (unsigned i = aaStart; i < aaEnd; i++)
		{
			currentCodonSpecificParameter[dEta][k][i] = proposedCodonSpecificParameter[dEta][k][i];
		}
	}
}





// -------------------------------------//
// ---------- Prior Functions ----------//
// -------------------------------------//
double ROCParameter::getMutationPriorStandardDeviation()
{
	return mutation_prior_sd;
}


void ROCParameter::setMutationPriorStandardDeviation(double _mutation_prior_sd)
{
	mutation_prior_sd = _mutation_prior_sd;
}





// ------------------------------------------------------------------//
// ---------- Posterior, Variance, and Estimates Functions ----------//
// ------------------------------------------------------------------//





double ROCParameter::getNoiseOffsetPosteriorMean(unsigned index, unsigned samples)
{
	double posteriorMean = 0.0;
	std::vector<double> NoiseOffsetTrace = traces.getSynthesisOffsetTrace(index);
	unsigned traceLength = lastIteration;

	if (samples > traceLength)
	{
#ifndef STANDALONE
		Rf_warning("Warning in ROCParameter::getNoiseOffsetPosteriorMean throws: Number of anticipated samples (%d) is greater than the length of the available trace (%d). Whole trace is used for posterior estimate! \n",
				samples, traceLength);
#else
		std::cerr << "Warning in ROCParameter::getNoiseOffsetPosteriorMean throws: Number of anticipated samples ("
		<< samples << ") is greater than the length of the available trace (" << traceLength << ")."
		<< "Whole trace is used for posterior estimate! \n";
#endif
		samples = traceLength;
	}
	unsigned start = traceLength - samples;
	for (unsigned i = start; i < traceLength; i++)
	{
		posteriorMean += NoiseOffsetTrace[i];
	}
	return posteriorMean / (double)samples;
}


double ROCParameter::getNoiseOffsetVariance(unsigned index, unsigned samples, bool unbiased)
{
	std::vector<double> NoiseOffsetTrace = traces.getSynthesisOffsetTrace(index);
	unsigned traceLength = lastIteration;
	if (samples > traceLength)
	{
#ifndef STANDALONE
		Rf_warning("Warning in ROCParameter::getNoiseOffsetVariance throws: Number of anticipated samples (%d) is greater than the length of the available trace (%d). Whole trace is used for posterior estimate! \n",
				samples, traceLength);
#else
		std::cerr << "Warning in Parameter::getNoiseOffsetVariance throws: Number of anticipated samples (" << samples
		<< ") is greater than the length of the available trace (" << traceLength << ")."
		<< "Whole trace is used for posterior estimate! \n";
#endif
		samples = traceLength;
	}
	double posteriorMean = getNoiseOffsetPosteriorMean(index, samples);

	double posteriorVariance = 0.0;

	unsigned start = traceLength - samples;
	for (unsigned i = start; i < traceLength; i++)
	{
		double difference = NoiseOffsetTrace[i] - posteriorMean;
		posteriorVariance += difference * difference;
	}
	double normalizationTerm = unbiased ? (1 / ((double)samples - 1.0)) : (1 / (double)samples);
	return normalizationTerm * posteriorVariance;
}


// ----------------------------------------------//
// ---------- Adaptive Width Functions ----------//
// ----------------------------------------------//

void ROCParameter::adaptNoiseOffsetProposalWidth(unsigned adaptationWidth)
{
	for (unsigned i = 0; i < getNumObservedPhiSets(); i++) {
		double acceptanceLevel = numAcceptForNoiseOffset[i] / (double)adaptationWidth;
		traces.updateSynthesisOffsetAcceptanceRatioTrace(i, acceptanceLevel);
		if (acceptanceLevel < 0.2)
		{
			std_NoiseOffset[i] *= 0.8;
		}
		if (acceptanceLevel > 0.3)
		{
			std_NoiseOffset[i] *= 1.2;
		}
		numAcceptForNoiseOffset[i] = 0u;
	}
}

// -------------------------------------//
// ---------- Other Functions ----------//
// -------------------------------------//


void ROCParameter::setNumObservedPhiSets(unsigned _phiGroupings)
{
	obsPhiSets = _phiGroupings;
	noiseOffset.resize(obsPhiSets, 0.1);
	noiseOffset_proposed.resize(obsPhiSets, 0.1);
	std_NoiseOffset.resize(obsPhiSets, 0.1);
	numAcceptForNoiseOffset.resize(obsPhiSets, 0);
	observedSynthesisNoise.resize(obsPhiSets, 1.0);
}


void ROCParameter::getParameterForCategory(unsigned category, unsigned paramType, std::string aa, bool proposal,
										   double *returnSet)
{
	std::vector<double> *tempSet;
	tempSet = (proposal ? &proposedCodonSpecificParameter[paramType][category] : &currentCodonSpecificParameter[paramType][category]);

	unsigned aaStart;
	unsigned aaEnd;
	SequenceSummary::AAToCodonRange(aa, aaStart, aaEnd, true);

	unsigned j = 0u;
	for (unsigned i = aaStart; i < aaEnd; i++, j++)
	{
		returnSet[j] = tempSet->at(i);
	}
}

// -----------------------------------------------------------------------------------------------------//
// ---------------------------------------- R SECTION --------------------------------------------------//
// -----------------------------------------------------------------------------------------------------//



#ifndef STANDALONE


//--------------------------------------------------//
// ---------- Constructors & Destructors ---------- //
//--------------------------------------------------//


ROCParameter::ROCParameter(std::vector<double> stdDevSynthesisRate, std::vector<unsigned> geneAssignment,
						std::vector<unsigned> _matrix, bool splitSer) : Parameter(22)
{
	unsigned _numMixtures = _matrix.size() / 2;
	std::vector<std::vector<unsigned>> thetaKMatrix;
	thetaKMatrix.resize(_numMixtures);

	unsigned index = 0;
	for (unsigned i = 0; i < _numMixtures; i++)
	{
		for (unsigned j = 0; j < 2; j++, index++)
		{
			thetaKMatrix[i].push_back(_matrix[index]);
		}
	}
	initParameterSet(stdDevSynthesisRate, _matrix.size() / 2, geneAssignment, thetaKMatrix, splitSer);
	initROCParameterSet();

}

ROCParameter::ROCParameter(std::vector<double> stdDevSynthesisRate, unsigned _numMixtures, std::vector<unsigned> geneAssignment,
							bool splitSer, std::string _mutationSelectionState) : Parameter(22)
{
	std::vector<std::vector<unsigned>> thetaKMatrix;
	initParameterSet(stdDevSynthesisRate, _numMixtures, geneAssignment, thetaKMatrix, splitSer, _mutationSelectionState);
	initROCParameterSet();
}





//---------------------------------------------------------------//
// ---------- Initialization, Restart, Index Checking ---------- //
//---------------------------------------------------------------//


void ROCParameter::initCovarianceMatrix(SEXP _matrix, std::string aa)
{
	std::vector<double> tmp;
	NumericMatrix matrix(_matrix);

	for(unsigned i = 0u; i < aa.length(); i++)	aa[i] = (char)std::toupper(aa[i]);

	unsigned aaIndex = SequenceSummary::aaToIndex.find(aa) -> second;
	unsigned numRows = matrix.nrow();
	std::vector<double> covMatrix(numRows * numRows);

	//NumericMatrix stores the matrix by column, not by row. The loop
	//below transposes the matrix when it stores it.
	unsigned index = 0;
	for (unsigned i = 0; i < numRows; i++)
	{
		for(unsigned j = i; j < numRows * numRows; j += numRows, index++)
		{
			covMatrix[index] = matrix[j];
		}
	}
	CovarianceMatrix m(covMatrix);
	m.choleskiDecomposition();
	covarianceMatrix[aaIndex] = m;
}


void ROCParameter::initMutation(std::vector<double> mutationValues, unsigned mixtureElement, std::string aa)
{
	//TODO: seperate out the R wrapper functionality and make the wrapper
	//currentMutationParameter
	bool check = checkIndex(mixtureElement, 1, numMixtures);
	if (check)
	{
		mixtureElement--;

		unsigned category = getMutationCategory(mixtureElement);
		aa[0] = (char) std::toupper(aa[0]);
                unsigned aaStart;
                unsigned aaEnd;
                SequenceSummary::AAToCodonRange(aa, aaStart, aaEnd, true);
                for (unsigned i = aaStart, j = 0; i < aaEnd; i++, j++)
		{
			currentCodonSpecificParameter[dM][category][i] = mutationValues[j];
		}
	}
}


void ROCParameter::initSelection(std::vector<double> selectionValues, unsigned mixtureElement, std::string aa)
{
	//TODO: seperate out the R wrapper functionality and make the wrapper
	//currentSelectionParameter
	bool check = checkIndex(mixtureElement, 1, numMixtures);
	if (check)
	{
		mixtureElement--;
		int category = getSelectionCategory(mixtureElement);

		aa[0] = (char) std::toupper(aa[0]);
                unsigned aaStart;
                unsigned aaEnd;
                SequenceSummary::AAToCodonRange(aa, aaStart, aaEnd, true);
                for (unsigned i = aaStart, j = 0; i < aaEnd; i++, j++)
                {
                    currentCodonSpecificParameter[dEta][category][i] = selectionValues[j];
                }
	}
}


// -----------------------------------//
// ---------- CSP Functions ----------//
// -----------------------------------//


std::vector<std::vector<double>> ROCParameter::getProposedMutationParameter()
{
	return proposedCodonSpecificParameter[dM];
}


std::vector<std::vector<double>> ROCParameter::getCurrentMutationParameter()
{
	return currentCodonSpecificParameter[dM];
}


std::vector<std::vector<double>> ROCParameter::getProposedSelectionParameter()
{
	return proposedCodonSpecificParameter[dEta];
}


std::vector<std::vector<double>> ROCParameter::getCurrentSelectionParameter()
{
	return currentCodonSpecificParameter[dEta];
}


void ROCParameter::setProposedMutationParameter(std::vector<std::vector<double>> _proposedMutationParameter)
{
	proposedCodonSpecificParameter[dM] = _proposedMutationParameter;
}


void ROCParameter::setCurrentMutationParameter(std::vector<std::vector<double>> _currentMutationParameter)
{
	currentCodonSpecificParameter[dM] = _currentMutationParameter;
}


void ROCParameter::setProposedSelectionParameter(std::vector<std::vector<double>> _proposedSelectionParameter)
{
	proposedCodonSpecificParameter[dEta] = _proposedSelectionParameter;
}


void ROCParameter::setCurrentSelectionParameter(std::vector<std::vector<double>> _currentSelectionParameter)
{
	currentCodonSpecificParameter[dEta] = _currentSelectionParameter;
}


// ------------------------------------------------------------------//
// ---------- Posterior, Variance, and Estimates Functions ----------//
// ------------------------------------------------------------------//





#endif





std::vector<double> ROCParameter::propose(std::vector<double> currentParam, double (*proposal)(double a, double b),
		double A, std::vector<double> B)
{
	unsigned _numParam = (unsigned)currentParam.size();
	std::vector<double> proposedParam(_numParam, 0.0);
	for (unsigned i = 0; i < _numParam; i++)
	{
		proposedParam[i] = (*proposal)(A + currentParam[i], B[i]);
	}
	return proposedParam;
}






