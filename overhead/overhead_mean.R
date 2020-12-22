#!/usr/bin/env Rscript
source_local <- function(fname){
    argv <- commandArgs(trailingOnly = FALSE)
    base_dir <- dirname(substring(argv[grep("--file=", argv)], 8))
    source(paste(base_dir, fname, sep="/"))
}
nogui <- TRUE
source_local("../../factoring/_helpers.R")
debug <- FALSE

cat(sprintf("# Command: %s\n", paste(commandArgs(), collapse=" ")))

infiles <- Sys.glob(commandArgs(trailingOnly=TRUE)[1])
outfile <- commandArgs(trailingOnly=TRUE)[2]

all_data <- NULL
nr_files <- 0
for (i in infiles) {
  i_data <- readSingleRuntimeMeasurementFile(i)

  if (is.null(all_data)) {
    # first data set
    all_data <- i_data
  }
  else {
    # element-wise sum
    all_data <- Map(`+`, all_data, i_data)
  }

  nr_files <- nr_files + 1
}

all_data <- unlist(all_data) / nr_files

for (value in all_data) {
  cat(sprintf("%f,,task-clock\n", value))
}

print(mean(all_data))