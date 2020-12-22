#!/usr/bin/env Rscript
source_local <- function(fname){
    argv <- commandArgs(trailingOnly = FALSE)
    base_dir <- dirname(substring(argv[grep("--file=", argv)], 8))
    source(paste(base_dir, fname, sep="/"))
}
source_local("../../factoring/_helpers.R")
debug <- FALSE

draw_legend <- FALSE
legend_position <- "none"

if (draw_legend) {
  legend_position <- c(1, 1)
}

# install_and_import("cowplot")
# theme_set(theme_cowplot(font_size=10))

install_and_import("ggrepel")
install_and_import("Cairo")

cat(sprintf("%% Command: %s\n", paste(commandArgs(), collapse=" ")))
cat(sprintf("%%\n"))

if (!debug) {
  library(extrafont)
  loadfonts()
}

# output_to_tex(paste(csvfile, '.tex', sep=''), width=4, height=4)

counter <- 0

all_data <- NULL
all_labels <- c()
shapes <- c()

# below_threshold_value <- 0.001
threshold <- 0.001
# not_executed_value <- 0.00000001

legend_title <- ""

args <- commandArgs(trailingOnly=TRUE)
idx <- 1

markdata <- c()

while (TRUE) {
  vanilla_time <- as.numeric(args[idx])
  idx <- idx+1
  bbl_file <- args[idx]
  idx <- idx+1

  while (TRUE) {
    if (idx > length(args)) {
      break
    }

    dataset <- args[idx]
    idx <- idx+1

    if (dataset == "") {
      break
    }

    print(dataset)

    overhead_factor <- as.numeric(args[idx]) # in milliseconds
    idx <- idx+1

    # BBL data
    data <- readcsv(bbl_file)
    names(data) <- c("address", "exec", "cantransform", "belowthreshold", "considerfunction")

    # sort by execution count
    data <- data[order(-data$exec), ]
    total <- nrow(data)

    # compute overhead for 150 instructions
    helper <- function(x) {
      x * overhead_factor / vanilla_time * 100
    }
    data$overhead <- mapply(helper, data$exec)

    regular <- data %>% filter(exec > 0)

    # print % executed
    print(nrow(regular)/total*100)
    print(max(data$overhead))

    # find 10% point
    x_coord <- round(0.10*nrow(regular))
    y_coord <- regular[x_coord, ]$overhead

    markdata <- append(markdata, c(x_coord, y_coord))

    # notexec <- data %>% filter(exec == 0)

    # cap on threshold
    procdata <- regular
    procdata$overhead[procdata$overhead < threshold] <- threshold

    # add index column
    procdata$index <- seq.int(nrow(procdata))

    # description, to be used in the legend as well
    descr <- dataset
    procdata$dataset <- descr

    nr_data <- nrow(procdata)

    # filter data points for fast PDF rendering
    selected_rows <- seq(from=1, to=1000, by=1) # seq = inclusive!
    if (nr_data < 10000) {
      selected_rows <- append(selected_rows, seq(from=1001, to=nr_data, by=5))
    } else {
      selected_rows <- append(selected_rows, seq(from=1001, to=10000, by=5))
      if (nr_data < 100000) {
        selected_rows <- append(selected_rows, seq(from=10000, to=nr_data, by=10))
      } else {
        selected_rows <- append(selected_rows, seq(from=10000, to=100000, by=10))
      }
    }

    filtered_data <- procdata[selected_rows, ]

    # print(tail(filtered_data, 20))

    # collect all data
    if (is.null(all_data)) {
      all_data <- filtered_data
    }
    else {
      all_data <<- rbind(all_data, filtered_data)
    }

    all_labels <- append(all_labels, descr)
    shapes <- append(shapes, c(20, 20))
  }

  if (idx > length(args)) {
    break
  }
}

g <- ggplot(NULL)

xmax <- 100000
ymax <- 100000
# py <- seq(from=0, to=ymax, by=20)
py <- c(0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000)
px <- c(1, 10, 100, 1000, 10000, 100000)

xlabformatter <- function(x) {
  lab <- sprintf('%d', x)
}

ylabformatter <- function(x) {
  result <- sprintf('%.3f', x)
  result <- gsub("0+$", "", result)
  result <- gsub("\\.$", "", result)
  lab <- result
}

colors <- c("red","red3", "dodgerblue","blue4", "chartreuse","chartreuse4", "grey40","black")
markersize <- 1

g <- g +
  geom_point(data=all_data, aes(x=index, y=overhead, color=dataset, shape=dataset), size=markersize) +
  scale_y_continuous(trans='log10', label=ylabformatter, breaks=py, limits=c(threshold, ymax), expand=expand_scale(c(0.01, 0.01)),) +
  scale_x_continuous(trans='log10', label=xlabformatter, breaks=px, limits=c(1, xmax), expand=expand_scale(0.01, 0.1)) +
  labs(x="Basic block index (1=hottest)", y="Estimated overhead (%)", fill=legend_title) +
  scale_color_manual(name = legend_title, values=colors) +
  scale_shape_manual(name = legend_title, values=shapes) +
  theme(legend.position=legend_position) +
  theme(legend.justification=c(1, 1), legend.title=element_blank(), legend.key=element_blank(), legend.key.size = unit(4, "mm"), text=element_text(family="Times New Roman", size=25/.pt)) +
  geom_hline(yintercept=threshold, color = "black", linetype="dashed", size=0.25)

markdata_idx <- 1
color_idx <- 1

# teken lijntjes om de pijlen te kunnen plaatsen in Inkscape
if (FALSE) {
  while (markdata_idx <= length(markdata)) {
    x <- markdata[markdata_idx+0]
    y <- markdata[markdata_idx+1]

    g <- g + geom_hline(yintercept=y, color=colors[color_idx], size=0.25)
    g <- g + geom_vline(xintercept=x, color=colors[color_idx], size=0.25)

    markdata_idx <- markdata_idx + 2
    color_idx <- color_idx + 1
  }
}

g

if (!debug) {
  if (draw_legend) {
    # Make JUST the legend points larger without changing the size of the legend lines:
    # To get a list of the names of all the grobs in the ggplot
    grid::grid.ls(grid::grid.force())

    # Set the size of the point in the legend to 2 mm
    grid::grid.gedit("key-[-0-9]-1-1", size = unit(2.5, "mm"))
    grid::grid.gedit("key-1[-0-9]-1-1", size = unit(2.5, "mm"))

    # save the modified plot to an object
    g <- grid::grid.grab()

    g
  }

  # Latex document:
  # \usepackage{layouts}
  # linewidth: \printinunitsof{mm}\prntlen{\linewidth}
  # textheight: \printinunitsof{mm}\prntlen{\textheight}
  ggsave('test.pdf', plot=g, device=cairo_pdf, units="mm", width=139, height=90, dpi="print")
} else {
  output_finish()
}

# cat ../usecases/radare2/protected/experiment/no_protection/b.out.all_bbls | sort -n -k2 -t ","
