## process the data
data(example)

motif_scores <-
  ComputeMotifScore(motif_library, snpInfo, ncores = 1)

motif_scores <-
  MatchSubsequence(
    motif_scores$snp.tbl,
    motif_scores$motif.scores,
    ncores = 1,
    motif.lib = motif_library
  )

len_seq <- sapply(motif_scores$ref_seq, nchar)
snp_pos <- as.integer(len_seq / 2) + 1


test_that("Error: reference bases are not the same as the sequence matrix.", {
  expect_equal(sum(snpInfo$sequence_matrix[31,] != snpInfo$ref_base), 0)
  expect_equal(sum(snpInfo$sequence_matrix[31,] == snpInfo$snp_base), 0)
})

test_that("Error: log_lik_ratio is not correct.", {
  expect_equal(motif_scores$log_lik_ref - motif_scores$log_lik_snp,
               motif_scores$log_lik_ratio)
})

test_that("Error: log likelihoods are not correct.", {
  log_lik <- sapply(seq(nrow(motif_scores)),
                    function(i) {
                      motif_mat <- motif_library[[motif_scores$motif[i]]]
                      colind <-
                        which(snpInfo$snpids == motif_scores$snpid[i])
                      bases <-
                        snpInfo$sequence_matrix[motif_scores$ref_start[i]:motif_scores$ref_end[i], colind]
                      if (motif_scores$ref_strand[i] == "-")
                        bases <- 5 - rev(bases)
                      log(prod(motif_mat[cbind(seq(nrow(motif_mat)),
                                               bases)]))
                    })
  
  expect_equal(log_lik, motif_scores$log_lik_ref, tolerance = 1e-5)
  
  snp_mat <- snpInfo$sequence_matrix
  snp_mat[cbind(snp_pos, seq(ncol(snp_mat)))] <- snpInfo$snp_base
  log_lik <- sapply(seq(nrow(motif_scores)),
                    function(i) {
                      motif_mat <- motif_library[[motif_scores$motif[i]]]
                      colind <-
                        which(snpInfo$snpids == motif_scores$snpid[i])
                      bases <-
                        snp_mat[motif_scores$snp_start[i]:motif_scores$snp_end[i], colind]
                      if (motif_scores$snp_strand[i] == "-")
                        bases <- 5 - rev(bases)
                      log(prod(motif_mat[cbind(seq(nrow(motif_mat)),
                                               bases)]))
                    })
  
  expect_equal(log_lik, motif_scores$log_lik_snp, tolerance = 1e-5)
})

test_that("Error: log_enhance_odds not correct.", {
  len_seq <- sapply(motif_scores$ref_seq, nchar)
  snp_pos <- as.integer(len_seq / 2) + 1
  
  ## log odds for reduction in binding affinity
  
  pos_in_pwm <- snp_pos - motif_scores$ref_start + 1
  neg_ids <- which(motif_scores$ref_strand == "-")
  pos_in_pwm[neg_ids] <-
    motif_scores$ref_end[neg_ids] - snp_pos[neg_ids] + 1
  snp_base <-
    sapply(substr(motif_scores$snp_seq, snp_pos, snp_pos), function(x)
      which(c("A", "C", "G", "T") == x))
  ref_base <-
    sapply(substr(motif_scores$ref_seq, snp_pos, snp_pos), function(x)
      which(c("A", "C", "G", "T") == x))
  snp_base[neg_ids] <- 5 - snp_base[neg_ids]
  ref_base[neg_ids] <- 5 - ref_base[neg_ids]
  my_log_reduce_odds <- sapply(seq(nrow(motif_scores)),
                               function(i)
                                 log(motif_library[[motif_scores$motif[i]]][pos_in_pwm[i], ref_base[i]]) -
                                 log(motif_library[[motif_scores$motif[i]]][pos_in_pwm[i], snp_base[i]]))
  
  expect_equal(my_log_reduce_odds, motif_scores$log_reduce_odds)
  
  ## log odds in enhancing binding affinity
  
  pos_in_pwm <- snp_pos - motif_scores$snp_start + 1
  neg_ids <- which(motif_scores$snp_strand == "-")
  pos_in_pwm[neg_ids] <-
    motif_scores$snp_end[neg_ids] - snp_pos[neg_ids] + 1
  snp_base <-
    sapply(substr(motif_scores$snp_seq, snp_pos, snp_pos), function(x)
      which(c("A", "C", "G", "T") == x))
  ref_base <-
    sapply(substr(motif_scores$ref_seq, snp_pos, snp_pos), function(x)
      which(c("A", "C", "G", "T") == x))
  snp_base[neg_ids] <- 5 - snp_base[neg_ids]
  ref_base[neg_ids] <- 5 - ref_base[neg_ids]
  my_log_enhance_odds <- sapply(seq(nrow(motif_scores)),
                                function(i)
                                  log(motif_library[[motif_scores$motif[i]]][pos_in_pwm[i], snp_base[i]]) -
                                  log(motif_library[[motif_scores$motif[i]]][pos_in_pwm[i], ref_base[i]]))
  
  expect_equal(my_log_enhance_odds, motif_scores$log_enhance_odds)
  
  
})

test_that("Error: the maximum log likelihood computation is not correct.", {
  snp_mat <- snpInfo$sequence_matrix
  snp_mat[cbind(snp_pos, seq(ncol(snp_mat)))] <- snpInfo$snp_base
  
  .AggLogLik <- function(seq_vec, pwm) {
    snp_pos <- as.integer(length(seq_vec) / 2) + 1
    start_pos <- snp_pos - nrow(pwm) + 1
    end_pos <- snp_pos
    rev_seq <- 5 - rev(seq_vec)
    
    subseq_log_probs <- rep(NA, (end_pos - start_pos + 1))
    for (i in start_pos:end_pos) {
      subseq_log_probs[2 * (i - start_pos) + 1] <-
        log(prod(pwm[cbind(seq(nrow(pwm)),
                           seq_vec[i - 1 + seq(nrow(pwm))])]))
      subseq_log_probs[2 * (i - start_pos) + 2] <-
        log(prod(pwm[cbind(seq(nrow(pwm)),
                           rev_seq[i - 1 + seq(nrow(pwm))])]))
    }
    return(c(
      max(subseq_log_probs),
      max(mean(subseq_log_probs[seq(1, length(subseq_log_probs), 2)]),
          mean(subseq_log_probs[seq(2, length(subseq_log_probs), 2)])),
      median(subseq_log_probs)
    ))
  }
  
  ## find the maximum log likelihood on the reference sequence
  my_log_lik_ref <- sapply(seq(nrow(motif_scores)),
                           function(x) {
                             colind <-
                               which(snpInfo$snpids == motif_scores$snpid[x])
                             seq_vec <-
                               snpInfo$sequence_matrix[, colind]
                             pwm <-
                               motif_library[[motif_scores$motif[x]]]
                             return(.AggLogLik(seq_vec, pwm))
                           })
  
  ## find the maximum log likelihood on the SNP sequence
  
  my_log_lik_snp <- sapply(seq(nrow(motif_scores)),
                           function(x) {
                             colind <- which(snpInfo$snpids == motif_scores$snpid[x]) #ADDED
                             seq_vec <- snp_mat[, colind]
                             pwm <-
                               motif_library[[motif_scores$motif[x]]]
                             return(.AggLogLik(seq_vec, pwm))
                           })
  
  expect_equal(my_log_lik_ref[1, ], motif_scores$log_lik_ref, tolerance =
                 1e-5)
  expect_equal(my_log_lik_snp[1, ], motif_scores$log_lik_snp, tolerance =
                 1e-5)
  expect_equal(my_log_lik_ref[2, ], motif_scores$mean_log_lik_ref, tolerance =
                 1e-5)
  expect_equal(my_log_lik_snp[2, ], motif_scores$mean_log_lik_snp, tolerance =
                 1e-5)
  expect_equal(my_log_lik_ref[3, ],
               motif_scores$median_log_lik_ref,
               tolerance = 1e-5)
  expect_equal(my_log_lik_snp[3, ],
               motif_scores$median_log_lik_snp,
               tolerance = 1e-5)
})
