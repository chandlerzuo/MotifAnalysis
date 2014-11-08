#include "MotifScore.h"

/*
Reference: Hock Peng Chan et al.(2010). Importance sampling of word patterns in DNA and protein sequences. Journal of computational biology, 17(12).
*/

/*
Compute the probability that a random sequence can get a score higher than 'score'.
@arg pwm The position weight matrix, with 4 columns corresponding to A, C, G, T.
@arg stat_dist A vector of length 4 with stationary distributions of A, C, G, T.
@arg trans_mat A 4 x 4 transition matrix.
@arg scores A matrix with 2 columns, with each column corresponding to one allele.
@arg p The upper percentile of the scores which is used as the mean of the importance sampling distribution.
@return A matrix with 3 columns. The first two columns are the p-values for the log-likelihood scores of each allele. The third column are the p-values for the likelihood ratios.
*/
NumericVector p_value_diff(NumericMatrix pwm, NumericMatrix wei_mat, NumericVector stat_dist, NumericMatrix trans_mat, NumericMatrix scores, double p) {
	double score_percentile = find_percentile_diff(scores, p);
	printf("percentile:%3.3f\n", score_percentile);
	// find the tilting parameter
	double theta = find_theta_diff(wei_mat, stat_dist, trans_mat, score_percentile);
	printf("theta:%3.3f\n", theta);
	NumericVector p_values(scores.nrow());
	NumericVector sample_score(4);
	int start_pos;

	double tol = 1e-10;
	int motif_len = pwm.nrow();
	NumericMatrix delta(4 * motif_len, motif_len);
	IntegerVector sample(2 * motif_len);
	IntegerVector sample_vec(2 * motif_len - 1);

	for(int i = 0; i < 4; i ++)
		for(int m = 0; m < motif_len; m ++) {
			if(pwm(m, i) < tol)
				pwm(m, i) = tol;
			if(wei_mat(m, i) < tol)
				wei_mat(m, i) = tol;
		}
	for(int pos = 0; pos < motif_len; pos ++) {
		for(int i = 0; i < 4; i ++) {
			delta(i + 4 * pos, motif_len - 1) = 1;
			if(theta < 0) {
				delta(i + 4 * pos, motif_len - 1) = 1 / pow(wei_mat(motif_len - 1 - pos, i), - theta);
			} else {
				delta(i + 4 * pos, motif_len - 1) = pow(wei_mat(motif_len - 1 - pos, i), theta);
			}
			//printf("delta(%d,%d)=%3.10f\n", i, motif_len - 1, delta(i, motif_len - 1));
		}
		// Formula (A.4)
		for(int m = motif_len - 2; m >= 0; m --) {
			for(int i = 0; i < 4; i ++) {
				delta(i + 4 * pos, m) = 0;
				for(int j = 0; j < 4; j ++) {
					delta(i + 4 * pos, m) += trans_mat(i, j) * delta(j + 4 * pos, m + 1);
				}
				if(delta(i + 4 * pos, m) < tol) {
					delta(i + 4 * pos, m) = tol;
				}
				//printf("delta(%d,%d)=%3.10f\n", i, m, delta(i, m));
			}
		}
	}

	double norm_const = 0;
	for(int i = 0; i < 4; i ++) {
		for(int pos = 0; pos < motif_len; pos ++) {
			norm_const += stat_dist[i] * delta(i + 4 * pos, 0);
		}
	}
	norm_const /= motif_len;
	printf("Constant value : %3.10f\n", norm_const);

	for(int i = 0; i < p_values.size(); i ++)
		p_values[i] = 0;
	
	int n_sample = 1e4;
	double mean_wei = 0;
	double wei = 0;
	double mean_diff = 0;
	double mean_score = 0;
	for(int i = 0; i < n_sample; i ++) {
		sample = importance_sample_diff(delta, stat_dist, trans_mat, wei_mat, theta);
		for(int j = 0; j < motif_len * 2 - 1; j ++) {
			sample_vec[j] = sample[j];
		}
		start_pos = sample[2 * motif_len - 1];
		sample_score = compute_sample_score_diff(pwm, wei_mat, sample_vec, start_pos);
		wei = norm_const / exp(theta * sample_score[0]);
		mean_wei += wei;
		mean_score += sample_score[0];
		for(int j = 0; j < scores.nrow(); j ++) {
			for(int k = 1; k < 4; k ++) {
				// SNP changes binding affinity
				if(j == 0) {
					mean_diff += sample_score[k];
				}
				if(sample_score[k] >= scores(j, 1) - scores(j, 0) && sample_score[k] >= scores(j, 0) - scores(j, 1))
					p_values[j] += wei;
			}
		}
	}
	printf("Mean weight : %lf \n", mean_wei / n_sample);
	printf("Mean diff score : %lf \n", mean_diff / 3 / n_sample);
	printf("Mean score : %lf \n", mean_score / n_sample);
	for(int j = 0; j < scores.nrow(); j ++) {
		p_values[j] /= 3 * n_sample;
	}
	return(p_values);
}

double func_delta_diff(NumericMatrix wei_mat, NumericVector stat_dist, NumericMatrix trans_mat, double theta) {
	int motif_len = wei_mat.nrow();
	NumericMatrix delta(4 * motif_len, motif_len);
	double tol = 1e-10;
	
	for(int pos = 0; pos < motif_len; pos ++) {
		for(int i = 0; i < 4; i ++) {
			delta(i + 4 * pos, motif_len - 1) = 1;
			if(theta < 0) {
				delta(i + 4 * pos, motif_len - 1) = 1 / pow(wei_mat(motif_len - 1 - pos, i), - theta);
			} else {
				delta(i + 4 * pos, motif_len - 1) = pow(wei_mat(motif_len - 1 - pos, i), theta);
			}
			//printf("delta(%d,%d)=%3.10f\n", i, motif_len - 1, delta(i, motif_len - 1));
		}
		// Formula (A.4)
		for(int m = motif_len - 2; m >= 0; m --) {
			for(int i = 0; i < 4; i ++) {
				delta(i + 4 * pos, m) = 0;
				for(int j = 0; j < 4; j ++) {
					delta(i + 4 * pos, m) += trans_mat(i, j) * delta(j + 4 * pos, m + 1);
				}
				if(delta(i + 4 * pos, m) < tol) {
					delta(i + 4 * pos, m) = tol;
				}
				//printf("delta(%d,%d)=%3.10f\n", i, m, delta(i, m));
			}
		}
	}

	double norm_const = 0;
	double const_copy = 0;
	for(int i = 0; i < 4; i ++) {
		for(int pos = 0; pos < motif_len; pos ++) {
			norm_const += stat_dist[i] * delta(i + 4 * pos, 0);
			const_copy += stat_dist[i] * delta(i + 4 * pos, motif_len - 1);
		}
	}
	//	printf("const %lf\t%lf\n", norm_const, const_copy);
	norm_const /= motif_len;
	
	return(norm_const);
}

/*
  Find the tilting paramter for the importance sampling distribution, using Equation (4.5).
*/
double find_theta_diff(NumericMatrix wei_mat, NumericVector stat_dist, NumericMatrix trans_mat, double score) {
	double theta = 0;
	double low_delta = log(func_delta_diff(wei_mat, stat_dist, trans_mat, theta - 0.005));
	double upp_delta = log(func_delta_diff(wei_mat, stat_dist, trans_mat, theta + 0.005));
	if(upp_delta - low_delta < score * 0.01) {
		while(upp_delta - low_delta < score * 0.01) {
			theta += 0.01;
			low_delta = upp_delta;
			upp_delta = log(func_delta_diff(wei_mat, stat_dist, trans_mat, theta + 0.005));
		}
	} else {
		while(upp_delta - low_delta > score * 0.01) {
			theta -= 0.01;
			upp_delta = low_delta;
			low_delta = log(func_delta_diff(wei_mat, stat_dist, trans_mat, theta - 0.005));
		}
	}
	return(theta);
}

IntegerVector importance_sample_diff(NumericMatrix delta, NumericVector stat_dist, NumericMatrix trans_mat, NumericMatrix wei_mat, double theta) {
	int motif_len = delta.ncol();
	// compute the sampling distribution for each coordinate
	// sample a random vector
	RNGScope scope;
	NumericVector rv = runif(2 * motif_len);
	// note: the last digit is for sampling the start position
	double prob_start[motif_len];
	for(int i = 0; i < motif_len; i ++) {
		prob_start[i] = 0;
		for(int j = 0; j < 4; j ++) {
			prob_start[i] += stat_dist[j] * delta(j + i * 4, 0);
		}
		if(i > 0)
			prob_start[i] += prob_start[i - 1];
	}
	double norm_const = prob_start[motif_len - 1];
	rv[2 * motif_len - 1] *= norm_const;
	int start_pos = 0;
	while(start_pos < motif_len - 1 && rv[2 * motif_len - 1] > prob_start[start_pos]) {
		start_pos ++;
	}
	// the subsequence of length motif_len starting from start_pos follows the importance sampling distribution
	// the rest of the subsequence follows the prior distribution
	if(start_pos == motif_len)
		start_pos = motif_len - 1;
	IntegerVector sample_vec(motif_len * 2);
	sample_vec[motif_len * 2 - 1] = start_pos;
	for(int i = 0; i < 2 * motif_len - 1; i ++) {
		double cond_prob[4];
		for(int j = 0; j < 4; j ++) {
			if(i == 0) {
				cond_prob[j] = stat_dist[j];
			} else {
				cond_prob[j] = trans_mat(sample_vec[i - 1], j);
			}
			if(i < motif_len) {
				cond_prob[j] *= delta(j + 4 * start_pos, i);
			}
			if(j > 0) {
				cond_prob[j] += cond_prob[j - 1];
			}
		}
		/*
		  if(i >= start_pos && i < start_pos + motif_len) {
			double test_prob[4];
			for(int j = 0; j < 4; j ++) {
				test_prob[j] = stat_dist[j];
				test_prob[j] *= pow(wei_mat(i - start_pos, j), theta);
				if(j > 0)
					test_prob[j] += test_prob[j - 1];
			}
			for(int j = 0; j < 4; j ++) {
				printf("%d,%d: prob = %3.10f \t %3.10f \n", i - start_pos, j, cond_prob[j] / cond_prob[3], test_prob[j] / test_prob[3]);
			}
		}
		*/
		rv[i] *= cond_prob[3];
		sample_vec[i] = 0;
		while(sample_vec[i] < 3 && rv[i] > cond_prob[sample_vec[i]]) {
			sample_vec[i] ++;
		}
		// index from 1
		// sample_vec[i] ++;
		//		printf("%d\t", sample_vec[i]);
	}
	//	printf("\n");
	return(sample_vec);
}

NumericVector compute_sample_score_diff(NumericMatrix pwm, NumericMatrix wei_mat, IntegerVector sample_vec, int start_pos) {
	// compute the reverse strand sequence
	int motif_len = pwm.nrow();
	IntegerVector rev_sample_vec(motif_len * 2 - 1);
	IntegerVector sample_vec_copy(motif_len * 2 - 1);
	IntegerVector rev_sample_vec_copy(motif_len * 2 - 1);
	for(int i = 0; i < motif_len * 2 - 1; i ++) {
		rev_sample_vec[i] = 3 - sample_vec[motif_len * 2 - 2 - i];
		sample_vec_copy[i] = sample_vec[i];
		rev_sample_vec_copy[i] = 3 - sample_vec[motif_len * 2 - 2 - i];
	}
	// compute the maximum score
	double rnd_score = pwm_log_prob(pwm, sample_vec, find_best_match(pwm, sample_vec));
	double rnd_score_rev = pwm_log_prob(pwm, rev_sample_vec, find_best_match(pwm, rev_sample_vec));
	if(rnd_score_rev > rnd_score)
		rnd_score = rnd_score_rev;
	// SNP score
	double snp_score[3];
	int snp_id = 0;
	double rnd_score_copy, rnd_score_rev_copy;
	for(int j = 0; j < 4; j ++) {
		if(sample_vec[motif_len - 1] == j)
			continue;
		sample_vec_copy[motif_len - 1] = j;
		rev_sample_vec_copy[motif_len - 1] = 3 - j;
		rnd_score_copy = pwm_log_prob(pwm, sample_vec_copy, find_best_match(pwm, sample_vec_copy));
		rnd_score_rev_copy = pwm_log_prob(pwm, rev_sample_vec_copy, find_best_match(pwm, rev_sample_vec_copy));
		if(rnd_score_rev_copy > rnd_score_copy)
			rnd_score_copy = rnd_score_rev_copy;
		snp_score[snp_id] = rnd_score - rnd_score_copy;
		snp_id ++;
	}
	if(snp_id != 3) {
		printf("Error: snp_id = %d\n", snp_id);
	}

	// compute the weight = prior density / importance sampling density
	// note: must use the score based on the true start_pos to compute the weight
	// this is a bug that took 2 days to fix!
	double adj_score = log(wei_mat(motif_len - 1 - start_pos, sample_vec[motif_len - 1]));
	// return value
	NumericVector ret(4);
	ret[0] = adj_score;
	ret[1] = snp_score[0];
	ret[2] = snp_score[1];
	ret[3] = snp_score[2];
	//	printf("score:%3.3f\tweight:%3.3f\tconstant:%3.3f\n", ret[0], ret[1], log(delta(0, 3)));
	return(ret);
}

double find_percentile_diff(NumericMatrix scores, double p) {
	// compute the 1% quantile among the scores
	int n_top = scores.nrow() * p + 1;
	// heap stores the smalles 1% of all scores
	double heap[n_top];
	// initialize the heap
	for(int i = 0; i < n_top; i ++) {
		heap[i] = -1e10;
	}
	double score_diff = 0;
	// use the heap structure to find the 1% quantile among scores
	for(int i = 0; i < scores.nrow(); i ++) {
		score_diff = scores(i, 0) - scores(i, 1);
		if(score_diff < 0)
			score_diff = - score_diff;
		if(heap[0] < score_diff)
			heap[0] = score_diff;
		int idx = 0;
		// sort the values in the heap
		while(1) {
			// no children
			if(2 * idx + 1 >= n_top)
				break;
			// only one child
			if(2 * idx + 2 == n_top) {
				if(heap[idx] > heap[idx * 2 + 1]) {
					double tmp = heap[idx];
					heap[idx] = heap[idx * 2 + 1];
					heap[idx * 2 + 1] = tmp;
					idx = idx * 2 + 1;
				} else {
					break;
				}
			}
			if(2 * idx + 2 < n_top) {
				// find the larger between the children
				int new_idx = idx * 2 + 1;
				if(heap[new_idx] > heap[new_idx + 1])
					new_idx ++;
				if(heap[idx] > heap[new_idx]) {
					double tmp = heap[idx];
					heap[idx] = heap[new_idx];
					heap[new_idx] = tmp;
					idx = new_idx;
				} else {
					break;
				}
			}
		}
	}
	return(- heap[0]);
}

SEXP test_find_percentile_diff(SEXP _scores, SEXP _p) {
	NumericMatrix scores(_scores);
	double p = as<double>(_p);
	double ret = find_percentile_diff(scores, p);
	return(wrap(ret));
}

SEXP test_p_value_diff(SEXP _pwm, SEXP _wei_mat, SEXP _stat_dist, SEXP _trans_mat, SEXP _scores, SEXP _p) {
	NumericMatrix pwm(_pwm);
	NumericMatrix wei_mat(_wei_mat);
	NumericVector stat_dist(_stat_dist);
	NumericMatrix trans_mat(_trans_mat);
	NumericMatrix scores(_scores);
	double p = as<double>(_p);
	
	NumericVector p_values = p_value_diff(pwm, wei_mat, stat_dist, trans_mat, scores, p);
	return(wrap(p_values));
}

SEXP test_find_theta_diff(SEXP _wei_mat, SEXP _stat_dist, SEXP _trans_mat, SEXP _score) {
	NumericMatrix wei_mat(_wei_mat);
	NumericVector stat_dist(_stat_dist);
	NumericMatrix trans_mat(_trans_mat);
	double score = as<double>(_score);

	double ret = find_theta_diff(wei_mat, stat_dist, trans_mat, score);
	return(wrap(ret));
}

SEXP test_func_delta_diff(SEXP _wei_mat, SEXP _stat_dist, SEXP _trans_mat, SEXP _theta) {
	NumericMatrix wei_mat(_wei_mat);
	NumericVector stat_dist(_stat_dist);
	NumericMatrix trans_mat(_trans_mat);
	double theta = as<double>(_theta);
	
	double ret = func_delta_diff(wei_mat, stat_dist, trans_mat, theta);
	return(wrap(ret));
}

SEXP test_importance_sample_diff(SEXP _delta, SEXP _stat_dist, SEXP _trans_mat, SEXP _wei_mat, SEXP _theta) {
	NumericMatrix delta(_delta);
	NumericVector stat_dist(_stat_dist);
	NumericMatrix trans_mat(_trans_mat);
	NumericMatrix wei_mat(_wei_mat);
	double theta = as<double>(_theta);
	return(wrap(importance_sample_diff(delta, stat_dist, trans_mat, wei_mat, theta)));
}

SEXP test_compute_sample_score_diff(SEXP _pwm, SEXP _wei_mat, SEXP _sample_vec, SEXP _start_pos) {
	NumericMatrix pwm(_pwm);
	NumericMatrix wei_mat(_wei_mat);
	IntegerVector sample_vec(_sample_vec);
	int start_pos = as<int>(_start_pos);
	return(wrap(compute_sample_score_diff(pwm, wei_mat, sample_vec, start_pos)));
}