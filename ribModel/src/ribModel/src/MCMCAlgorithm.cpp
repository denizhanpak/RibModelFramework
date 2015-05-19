#include "../include/MCMCAlgorithm.h"
#include "../include/CovarianceMatrix.h"

#include <random>
#include <cstdlib>
#include <thread>
#include <sstream>

#include <iostream>
#include <fstream>
#include <stdlib.h> //can be removed later

#include <omp.h>



MCMCAlgorithm::MCMCAlgorithm()
{
    MCMCAlgorithm(1000, 1, true, true, true);
    adaptiveWidth = 100;
}

MCMCAlgorithm::MCMCAlgorithm(int _samples, int _thining, bool _estimateExpression, bool _estimateCodonSpecificParameter, bool _estimateHyperParameter)
    : samples(_samples), thining(_thining), estimateExpression(_estimateExpression), estimateCodonSpecificParameter(_estimateCodonSpecificParameter),
        estimateHyperParameter(_estimateHyperParameter)
{
    likelihoodTrace.resize(samples + 1);
    adaptiveWidth = 100;
}

MCMCAlgorithm::~MCMCAlgorithm()
{
    //dtor
}

MCMCAlgorithm::MCMCAlgorithm(const MCMCAlgorithm& other)
{
        samples = other.samples;
        thining = other.thining;
        adaptiveWidth = other.adaptiveWidth;

        estimateExpression = other.estimateExpression;
        estimateCodonSpecificParameter = other.estimateCodonSpecificParameter;
        estimateHyperParameter = other.estimateHyperParameter;

        likelihoodTrace = other.likelihoodTrace;
}

double MCMCAlgorithm::acceptRejectExpressionLevelForAllGenes(Genome& genome, ROCParameter& parameter, ROCModel& model, int iteration)
{
    // TODO move the likelihood calculation out off here. make it a void function again.

    double logLikelihood = 0.0;
    int numGenes = genome.getGenomeSize();

    // just for testing
    //unsigned concurentThreadsSupported = std::thread::hardware_concurrency();
    //omp_set_num_threads(concurentThreadsSupported);
    //#pragma omp parallel for //shared(parameter)
    // testing end
    unsigned numMixtures = parameter.getNumMixtureElements();

    double dirichletParameters[numMixtures];
		//initialize parameter's size
    for(int i = 0; i < numGenes; i++)
    {
        Gene gene = genome.getGene(i);
        double currLogLike = 0.0;
        double propLogLike = 0.0;


        double unscaledLogProb_curr[numMixtures];
        double unscaledLogProb_prop[numMixtures];
        double avgValue = 0.0;


        /*
            Since some values returned by calculateLogLiklihoodRatioPerGene are veyr small (~ -1100), exponantiation leads to 0.
            To solve this problem, we adjust the value by a constant c. I choose to use the average value across all mixtures.
            We justify this by
            P = Sum(p_i*f(...))
                => f' = c*f
                => ln(f') = ln(c) + ln(f)
                => ln(P) = ln( Sum(p_i*f'(...)) )
                => ln(P) = ln(P') - ln(c)
            Note that we use invere sign because our values of ln(f) and ln(f') are negative.
        */
        for(unsigned k = 0; k < numMixtures; k++)
        {
            // logProbabilityRatio contains the logProbabilityRatio in element 0,
            // the current unscaled probability in elemen 1 and the proposed unscaled probability in element 2
            double logProbabilityRatio[3];
            model.calculateLogLiklihoodRatioPerGene(gene, i, parameter, k, logProbabilityRatio);
            // store values so they can be processed
            unscaledLogProb_curr[k] = logProbabilityRatio[1];
            unscaledLogProb_prop[k] = logProbabilityRatio[2];
            // get average value of all return values which is used as constant c
            avgValue += logProbabilityRatio[1];
            avgValue += logProbabilityRatio[2];
        }
        avgValue /= 2*numMixtures; // average of logProbabilities

        // adjust the the unscaled probabilities by the constant c
        // ln(f') = ln(c) + ln(f)
        for(unsigned k = 0; k < numMixtures; k++)
        {
            unscaledLogProb_curr[k] -= avgValue;
            unscaledLogProb_prop[k] -= avgValue;
        }
        double logProbabilities[numMixtures];
        double normalizingProbabilityConstant = 0.0;

        // calculate ln(P) = ln( Sum(p_i*f'(...)) ) and obtain normalizing constant for new p_i
        for(unsigned k = 0; k < numMixtures; k++)
        {
            logProbabilities[k] = std::log( parameter.getCategoryProbability(k) * std::exp(unscaledLogProb_curr[k]) );
            currLogLike += logProbabilities[k];
            propLogLike += std::log( parameter.getCategoryProbability(k) * std::exp(unscaledLogProb_prop[k]) );
            normalizingProbabilityConstant += std::exp(logProbabilities[k]);
        }
        // normalize probabilities
        double probabilities[numMixtures];
        for (int k = 0; k < numMixtures; k++)
        {
            probabilities[k] = std::exp(logProbabilities[k]) / normalizingProbabilityConstant;
        }
        // Get category in which the gene is placed in.
        // If we use multiple sequence observrvation (like different mutatnts) randMultinom needs an parameter N to place N observations in numMixture buckets
        unsigned categoryOfGene = ROCParameter::randMultinom(probabilities, numMixtures);
        parameter.setMixtureAssignment(i, categoryOfGene);
        dirichletParameters[categoryOfGene] += 1;

        // accept/reject proposed phi values
        if( ( (double)std::rand() / (double)RAND_MAX ) < (std::exp(propLogLike) / std::exp(currLogLike)) )
        {
            // moves proposed phi to current phi
            //std::cout << "Update expression for Gene i = " << i << " in iteration " << iteration << std::endl;
            parameter.updateExpression(i);
            //#pragma omp atomic
            logLikelihood += propLogLike;
        }else{
            //#pragma omp atomic
            logLikelihood += currLogLike;
        }
        if((iteration % thining) == 0)
        {
            parameter.updateExpressionTrace(iteration/thining, i);
            parameter.updateMixtureAssignmentTrace(iteration/thining, i);
        }
    }
    //for (int a = 0; a < numMixtures; a++)
    //	 if (dirichletParameters[a] != 448) std::cout <<"ERROR\n";
    double newMixtureProbabilities[numMixtures];
    ROCParameter::randDirichlet(dirichletParameters, numMixtures, newMixtureProbabilities);
    for(unsigned k = 0u; k < numMixtures; k++)
    {
      parameter.setCategoryProbability(k, newMixtureProbabilities[k]);
    }
    if((iteration % thining) == 0)
    {
        parameter.updateCategoryProbabilitiesTrace(iteration/thining);
    }
    return logLikelihood;
}

void MCMCAlgorithm::acceptRejectHyperParameter(int numGenes, ROCParameter& parameter, ROCModel& model, int iteration)
{
    double logProbabilityRatio = 0.0;

    double currentSphi = parameter.getSphi(false);
    double currentMPhi = -(currentSphi * currentSphi) / 2;

    double proposedSphi = parameter.getSphi(true);
    double proposedMPhi = -(proposedSphi * proposedSphi) / 2;

    for(int i = 0; i < numGenes; i++)
    {
        unsigned mixture = parameter.getMixtureAssignment(i);
        double phi = parameter.getExpression(i, mixture, false);
        logProbabilityRatio += std::log(ROCParameter::densityLogNorm(phi, proposedMPhi, proposedSphi)) - std::log(ROCParameter::densityLogNorm(phi, currentMPhi, currentSphi));
    }

    logProbabilityRatio -= (std::log(currentSphi) - std::log(proposedSphi));


    if( -ROCParameter::randExp(1) < logProbabilityRatio )
    {
        // moves proposed Sphi to current Sphi
        parameter.updateSphi();
    }
    if((iteration % thining) == 0)
    {
        parameter.updateSphiTrace(iteration/thining);
    }
}

void MCMCAlgorithm::acceptRejectCodonSpecificParameter(Genome& genome, ROCParameter& parameter, ROCModel& model, int iteration)
{
    double logAcceptanceRatioForAllMixtures = 0.0;

    for(unsigned i = 0; i < 22; i++)
    {
        char curAA = SequenceSummary::AminoAcidArray[i];
        // skip amino acids with only one codon or stop codons
        if(curAA == 'X' || curAA == 'M' || curAA == 'W') continue;
        // calculate likelihood ratio for every Category for current AA
        model.calculateLogLikelihoodRatioPerAAPerCategory(curAA, genome, parameter, logAcceptanceRatioForAllMixtures);
        //std::cout << "logAcceptanceRatioForAllMixtures: " << logAcceptanceRatioForAllMixtures << "\n";
        if( -ROCParameter::randExp(1) < logAcceptanceRatioForAllMixtures )
        {
            // moves proposed codon specific parameters to current codon specific parameters
            parameter.updateCodonSpecificParameter(curAA);
        }
        if((iteration % thining) == 0)
        {
            //parameter.updateSphiTrace(iteration/thining);
        }
    }
}

void MCMCAlgorithm::run(Genome& genome, ROCModel& model, ROCParameter& parameter)
{
    unsigned maximumIterations = samples * thining;
    // initialize everything

    parameter.initAllTraces(samples, genome.getGenomeSize());
    CovarianceMatrix covmat = CovarianceMatrix(2);
    // starting the MCMC
    std::cout << "entering MCMC loop" << std::endl;
    std::cout << "Starting MCMC with " << maximumIterations << " iterations\n";
    for(unsigned iteration = 0; iteration < maximumIterations; iteration++)
    {

        if(iteration % 100 == 0) {std::cout << iteration << std::endl;}
        if(estimateCodonSpecificParameter) //should the "is" functions be used here instead?
        {
            parameter.proposeCodonSpecificParameter();
            acceptRejectCodonSpecificParameter(genome, parameter, model, iteration);
            if(iteration % adaptiveWidth == 0)
            {
                std::cout << "\n ================ \n";
                covmat.printCovarianceMatrix();
                std::cout << " ---------------- \n";
                covmat.calculateCovarianceMatrixFromTraces(parameter.getExpressionTrace(), 0, adaptiveWidth);
                covmat.printCovarianceMatrix();
                std::cout << " ================ \n";
            }

        }
        // update hyper parameter
        if(estimateHyperParameter)
        {
            parameter.proposeSPhi();
            acceptRejectHyperParameter(genome.getGenomeSize(), parameter, model, iteration);
            if( ( (iteration + 1) % adaptiveWidth) == 0)
            {
               parameter.adaptSphiProposalWidth(adaptiveWidth);
            }
        }
        // update expression level values
        if(estimateExpression)
        {
            parameter.proposeExpressionLevels();
            double logLike = acceptRejectExpressionLevelForAllGenes(genome, parameter, model, iteration);
            if((iteration % thining) == 0)
            {
                likelihoodTrace[iteration/thining] = logLike;
            }
            if((iteration % adaptiveWidth) == 0)
            {
                //std::cout <<"would call adaptExpressionPro.....\n";
               //parameter.adaptExpressionProposalWidth(adaptiveWidth);
            }
        }
    } // end MCMC loop
    std::cout << "leaving MCMC loop" << std::endl;

    // development output
    std::vector<std::vector<std::vector<double>>> expressionTrace = parameter.getExpressionTrace();

    for(int nm = 0u; nm < parameter.getNumMixtureElements(); nm++)
    {
        std::stringstream oss;
        oss << "results/phiTrace_nmix_" << nm << ".csv";
        std::ofstream phitraceout(oss.str(), std::ofstream::out);
        for(unsigned iteration = 0; iteration < samples; iteration++)
        {
            for(int i = 0; i < genome.getGenomeSize(); i++)
            {
                phitraceout << expressionTrace[0][iteration][i] << ",";
            }
            phitraceout << std::endl;
        }
        phitraceout.close();
    }

    std::ofstream likout("results/test.lik", std::ofstream::out);
    std::ofstream sphiout("results/test.sphi", std::ofstream::out);
    std::vector<double> sphiTrace = parameter.getSPhiTrace();
    for(unsigned iteration = 0; iteration < samples; iteration++)
    {
        likout << likelihoodTrace[iteration] << std::endl;
        sphiout << sphiTrace[iteration] << std::endl;
    }
    likout.close();
    sphiout.close();
}

