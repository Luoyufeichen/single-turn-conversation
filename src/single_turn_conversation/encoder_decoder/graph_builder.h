#ifndef SINGLE_TURN_CONVERSATION_SRC_ENCODER_DECODER_GRAPH_BUILDER_H
#define SINGLE_TURN_CONVERSATION_SRC_ENCODER_DECODER_GRAPH_BUILDER_H

#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <tuple>
#include <queue>
#include <algorithm>
#include <boost/format.hpp>
#include "N3LDG.h"
#include "model_params.h"
#include "hyper_params.h"
#include "single_turn_conversation/encoder_decoder/global_context/global_context_decoder_components.h"

struct WordIdAndProbability {
    int word_id;
    dtype probability;

    WordIdAndProbability() = default;
    WordIdAndProbability(const WordIdAndProbability &word_id_and_probability) = default;
    WordIdAndProbability(int wordid, dtype prob) : word_id(wordid), probability(prob) {}
};

std::vector<std::shared_ptr<DecoderComponents>> CopyDecoderComponents(
        const std::vector<std::shared_ptr<DecoderComponents>> &components) {
    std::vector
}

struct BeamSearchResult {
    int beam_i;
    std::vector<WordIdAndProbability> path;
    dtype final_log_probability;

    BeamSearchResult() = default;
    BeamSearchResult(const BeamSearchResult &beam_search_result) = default;
    BeamSearchResult(int beami, const std::vector<WordIdAndProbability> &pathh,
            dtype log_probability) : beam_i(beami), path(pathh),
    final_log_probability(log_probability) {}
};

std::vector<BeamSearchResult> mostLikeResults(
        const std::vector<Node *> &nodes,
        const std::vector<BeamSearchResult> &last_results) {
    if (nodes.size() != last_results.size() && !last_results.empty()) {
        std::cerr << boost::format(
                "nodes size is not equal to last_results size, nodes size is %1% but last_results size is %2%")
            % nodes.size() % last_results.size() << std::endl;
        abort();
    }

    int k = nodes.size();

    auto cmp = [](const BeamSearchResult &a, const BeamSearchResult &b) {
        return a.final_log_probability > b.final_log_probability;
    };
    std::priority_queue<BeamSearchResult, std::vector<BeamSearchResult>, decltype(cmp)> queue(cmp);
    for (int i = 0; i < nodes.size(); ++i) {
        const Node &node = *nodes.at(i);
        auto tuple = toExp(node);

        for (int j = 0; j < nodes.at(i)->dim; ++j) {
            dtype value = node.val.v[j] - std::get<1>(tuple).second;
            dtype log_probability = value - log(std::get<2>(tuple));
            dtype word_probability = exp(log_probability);
            std::vector<WordIdAndProbability> word_ids;
            if (!last_results.empty()) {
                log_probability += last_results.at(i).final_log_probability;
                word_ids = last_results.at(i).path;
            }

            word_ids.push_back(WordIdAndProbability(j, word_probability));
            BeamSearchResult beam_search_result(i, word_ids, log_probability);

            if (queue.size() < k) {
                queue.push(beam_search_result);
            } else if (queue.top().final_log_probability < log_probability) {
                queue.pop();
                queue.push(beam_search_result);
            }
        }
    }

    std::vector<BeamSearchResult> results;

    while (!queue.empty()) {
        auto &e = queue.top();
        results.push_back(e);
        queue.pop();
    }

    return results;
}

struct GraphBuilder {
    std::vector<std::shared_ptr<LookupNode>> encoder_lookups;
    DynamicLSTMBuilder encoder;
    BucketNode hidden_bucket;
    BucketNode word_bucket;

    void init(const HyperParams &hyper_params) {
        hidden_bucket.init(hyper_params.hidden_dim, -1);
        word_bucket.init(hyper_params.word_dim, -1);
    }

    void forward(Graph &graph, const std::vector<std::string> &sentence,
            const HyperParams &hyper_params,
            ModelParams &model_params) {
        hidden_bucket.forward(graph);
        word_bucket.forward(graph);

        for (const std::string &word : sentence) {
            std::shared_ptr<LookupNode> input_lookup(new LookupNode);
            input_lookup->init(hyper_params.word_dim, hyper_params.dropout);
            input_lookup->setParam(model_params.lookup_table);
            input_lookup->forward(graph, word);
            encoder_lookups.push_back(input_lookup);
        }

        for (std::shared_ptr<LookupNode> &node : encoder_lookups) {
            encoder.forward(graph, model_params.encoder_params, *node, hidden_bucket,
                    hidden_bucket);
        }
    }

    void forwardDecoder(Graph &graph, DecoderComponents &decoder_components,
            const std::vector<std::string> &answer,
            const HyperParams &hyper_params,
            ModelParams &model_params) {
        if (!graph.train) {
            std::cerr << "train should be true" << std::endl;
            abort();
        }

        for (int i = 0; i < answer.size(); ++i) {
            forwardDecoderByOneStep(graph, decoder_components, i,
                    i == 0 ? nullptr : &answer.at(i - 1),
                    hyper_params,
                    model_params);
        }
    }

    void forwardDecoderByOneStep(Graph &graph, DecoderComponents &decoder_components, int i,
            const std::string *answer,
            const HyperParams &hyper_params,
            ModelParams &model_params) {
        Node *last_input;
        if (i > 0) {
            std::shared_ptr<LookupNode> decoder_lookup(new LookupNode);
            decoder_lookup->init(hyper_params.word_dim, -1);
            decoder_lookup->setParam(model_params.lookup_table);
            decoder_lookup->forward(graph, *answer);
            decoder_components.decoder_lookups.push_back(decoder_lookup);
            last_input = decoder_components.decoder_lookups.at(i - 1).get();
        } else {
            last_input = &word_bucket;
        }

        decoder_components.decoder.forward(graph, model_params.decoder_params, *last_input, 
                *encoder._hiddens.at(encoder._hiddens.size() - 1),
                *encoder._cells.at(encoder._hiddens.size() - 1));

        std::shared_ptr<LinearNode> decoder_to_wordvector(new LinearNode);
        decoder_to_wordvector->init(hyper_params.word_dim, -1);
        decoder_to_wordvector->setParam(model_params.hidden_to_wordvector_params);
        decoder_to_wordvector->forward(graph, *decoder_components.decoder._hiddens.at(i));
        decoder_components.decoder_to_wordvectors.push_back(decoder_to_wordvector);

        std::shared_ptr<LinearWordVectorNode> wordvector_to_onehot(new LinearWordVectorNode);
        wordvector_to_onehot->init(model_params.lookup_table.nVSize, -1);
        wordvector_to_onehot->setParam(model_params.lookup_table.E);
        wordvector_to_onehot->forward(graph, *decoder_to_wordvector);
        decoder_components.wordvector_to_onehots.push_back(wordvector_to_onehot);
    }

    std::pair<std::vector<WordIdAndProbability>, dtype> forwardDecoderUsingBeamSearch(Graph &graph,
            const std::vector<std::shared_ptr<DecoderComponents>> &decoder_components_beam,
            const HyperParams &hyper_params,
            ModelParams &model_params) {
        if (graph.train) {
            std::cerr << "train should be false" << std::endl;
            abort();
        }
        auto beam = decoder_components_beam;
        std::vector<std::pair<std::vector<WordIdAndProbability>, dtype>> word_ids_result;
        std::vector<BeamSearchResult> most_like_results;
        std::vector<std::string> last_answers;

        for (int i = 0;; ++i) {
            last_answers.clear();
            if (i > 0) {
                std::vector<Node *> last_outputs;
                int beam_i = 0;
                for (std::shared_ptr<DecoderComponents> &decoder_components : beam) {
                    last_outputs.push_back(
                            decoder_components->wordvector_to_onehots.at(i - 1).get());
                    ++beam_i;
                }
                most_like_results = mostLikeResults(last_outputs, most_like_results);
                auto last_beam = beam;
                beam.clear();
                std::vector<BeamSearchResult> stop_removed_results;
                int j = 0;
                for (BeamSearchResult &beam_search_result : most_like_results) {
                    const std::vector<WordIdAndProbability> &word_ids = beam_search_result.path;
                    int last_word_id = word_ids.at(word_ids.size() - 1).word_id;
                    const std::string &word = model_params.lookup_table.elems->from_id(
                            last_word_id);
                    if (word == STOP_SYMBOL || i >= 100) {
//                        std::cout << boost::format(
//                                "i:%1% word:%2% most_like_results size:%3% j:%4%") % i % word %
//                            most_like_results.size() % j << std::endl;
                        word_ids_result.push_back(std::make_pair(word_ids,
                                    beam_search_result.final_log_probability));
                    } else {
                        stop_removed_results.push_back(beam_search_result);
                        last_answers.push_back(word);
                        beam.push_back(last_beam.at(beam_search_result.beam_i));
                    }
                    ++j;
                }
//                std::cout << boost::format("beam size:%1%") % beam.size() << std::endl;

                most_like_results = stop_removed_results;
            }

            if (beam.empty()) {
                break;
            }

            for (int beam_i = 0; beam_i < beam.size(); ++beam_i) {
                DecoderComponents &decoder_components = beam.at(beam_i);
                forwardDecoderByOneStep(graph, decoder_components, i,
                        i == 0 ? nullptr : &last_answers.at(beam_i),
                        hyper_params,
                        model_params);
            }

            graph.compute();
        }

        if (word_ids_result.size() != decoder_components_beam.size()) {
            std::cerr << boost::format("word_ids_result size is %1%, but beam_size is %2%") %
                word_ids_result.size() % decoder_components_beam.size() << std::endl;
            abort();
        }

        auto compair = [](const std::pair<std::vector<WordIdAndProbability>, dtype> &a,
                const std::pair<std::vector<WordIdAndProbability>, dtype> &b) {
            return a.second > b.second;
        };
        auto max = std::max_element(word_ids_result.begin(), word_ids_result.end(), compair);

        return std::make_pair(max->first, exp(max->second));
    }
};

#endif
