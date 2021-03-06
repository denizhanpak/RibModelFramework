% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/parameterObject.R
\name{getSelectionCoefficients}
\alias{getSelectionCoefficients}
\title{Calculate Selection coefficients}
\usage{
getSelectionCoefficients(genome, parameter, samples = 100)
}
\arguments{
\item{genome}{A genome object initialized with 
\code{\link{initializeGenomeObject}} to add observed expression data.}

\item{parameter}{an object created by \code{initializeParameterObject}.}

\item{samples}{The number of samples used for the posterior estimates.}
}
\value{
A matrix with selection coefficients.
}
\description{
\code{getSelectionCoefficients} calculates the selection coefficient of each codon in each gene.
}
\examples{
genome_file <- system.file("extdata", "genome.fasta", package = "AnaCoDa")

genome <- initializeGenomeObject(file = genome_file)
sphi_init <- 1
numMixtures <- 1
geneAssignment <- rep(1, length(genome))
parameter <- initializeParameterObject(genome = genome, sphi = sphi_init, 
                                       num.mixtures = numMixtures, 
                                       gene.assignment = geneAssignment, 
                                       mixture.definition = "allUnique")
model <- initializeModelObject(parameter = parameter, model = "ROC")
samples <- 2500
thinning <- 50
adaptiveWidth <- 25
mcmc <- initializeMCMCObject(samples = samples, thinning = thinning, 
                             adaptive.width=adaptiveWidth, est.expression=TRUE, 
                             est.csp=TRUE, est.hyper=TRUE, est.mix = TRUE) 
divergence.iteration <- 10
\dontrun{
runMCMC(mcmc = mcmc, genome = genome, model = model, 
        ncores = 4, divergence.iteration = divergence.iteration)

## return estimates for selection coefficients s for each codon in each gene
selection.coefficients <- getSelectionCoefficients(genome = genome, 
                                                   parameter = parameter, samples = 1000)
}

}
