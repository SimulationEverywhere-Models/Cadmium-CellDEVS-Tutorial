/**
 * Copyright (c) 2020, Román Cárdenas Rodríguez
 * ARSLab - Carleton University
 * GreenLSI - Polytechnic University of Madrid
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CELLDEVS_TUTORIAL_1_4_SPATIAL_SIRDS_SIRDS_CELL_HPP
#define CELLDEVS_TUTORIAL_1_4_SPATIAL_SIRDS_SIRDS_CELL_HPP

#include <cmath>
#include <nlohmann/json.hpp>
#include <cadmium/celldevs/cell/grid_cell.hpp>
#include "../state.hpp"
#include "../vicinity.hpp"

using namespace cadmium::celldevs;

/**
 * Configuration for basic Susceptible-Infected-Recovered model for Cadmium Cell-DEVS
 */
struct sirds_cell_config {
    float virulence;    /// in this example, virulence is provided using a configuration structure
    float recovery;     /// in this example, recovery is provided using a configuration structure
    float immunity;     /// in this example, immunity is provided using a configuration structure
    float fatality;     /// in this example, fatality is provided using a configuration structure
};

/**
 * We need to implement the from_json method for the desired cell configuration struct.
 * Otherwise, Cadmium will not be able to understand the JSON configuration file.
 * @param j Chunk of JSON file that represents a cell configuration
 * @param s cell configuration struct to be filled with the configuration shown in the JSON file.
 */
void from_json(const nlohmann::json& j, sirds_cell_config &c) {
    j.at("virulence").get_to(c.virulence);
    j.at("recovery").get_to(c.recovery);
    j.at("immunity").get_to(c.immunity);
    j.at("fatality").get_to(c.fatality);
}

/**
 * Basic Susceptible-Infected-Recovered model for Cadmium Cell-DEVS
 * @tparam T data type used to represent the simulation time
 */
template <typename T>
/// sirds_cell inherits the grid_cell class. As specified by the template, cell state uses the sir struct, and vicinities the mc struct
class [[maybe_unused]] sirds_cell : public grid_cell<T, sird, mc> {
public:
    // We must specify which attributes of the base class we are going to use
    using grid_cell<T, sird, mc>::simulation_clock;
    using grid_cell<T, sird, mc>::state;
    using grid_cell<T, sird, mc>::map;
    using grid_cell<T, sird, mc>::neighbors;

    sirds_cell_config cell_config;

    sirds_cell() : grid_cell<T, sird, mc>() {}

    [[maybe_unused]] sirds_cell(cell_position const &cell_id, cell_unordered<mc> const &neighborhood, sird initial_state,
                               cell_map<sird, mc> const &map_in, std::string const &delay_id, sirds_cell_config config) :
            grid_cell<T, sird, mc>(cell_id, neighborhood, initial_state, map_in, delay_id), cell_config(config) {
    }

    /**
     * We have to override the local_computation method to specify how the cell state changes according to our model.
     * Remember: the local_computation function CANNOT change any attribute of the cell object (it is a constant method)
     *           the local_computation function must return the state that the cell should have according to its current state and the neighbors' latest published state.
     * IMPORTANT: this function does not set the new state of the cell. It just says which state should have the cell. The Cadmium simulator will change the state when it applies
     * IMPORTANT: neighbor cells' state ARE JUST COPIES of their latest published state. You cannot change a neighbor cell state.
     * IMPORTANT: neighbor cells' latest published state MAY NOT BE the neighbor cells' current state.
     * @return the new state that the cell should have
     */
    [[nodiscard]] sird local_computation() const override {
        sird res = state.current_state;  // first, we make a copy of the cell's current state and store it in the variable res
        float new_i = new_infections(res);  // to compute the percentage of new infections, we implement an auxiliary method.
        float new_r = new_recoveries(res);  // to compute the percentage of new recovered people, we implement an auxiliary method
        float new_d = new_deceases(res);      // to compute the percentage of new dead people, we implement an auxiliary method
        float new_s = new_susceptibles(res);  // to compute the percentage of new susceptible people, we implement an auxiliary method

        // We just want two decimals in the percentage -> let's round the current outcome:
        res.deceased = std::round((res.deceased + new_d) * 100) / 100;
        res.recovered = std::round((res.recovered + new_r - new_s) * 100) / 100;
        res.infected = std::round((res.infected + new_i - new_r - new_d) * 100) / 100;
        res.susceptible = 1 - res.infected - res.recovered - res.deceased;
        // We return the new state that the cell should have (remember, it is not yet the cell's state)
        return res;
    }

    /**
     * We have to override the output_delay function to tell how long we have to wait before sending a copy of the cell state to neighboring cells.
     * @param cell_state the new cell state.
     * @return how long the cell will wait before sending the new state to neighboring cells.
     */
    T output_delay(sird const &cell_state) const override {
        return 1;  // in this example, the delay is always 1 simulation tick.
    }

    /**
     * Auxiliary method to compute the percentage of new infections. This method MUST be constant. Otherwise, it won't compile
     * @param c_state current state of the cell
     * @return percentage of new infections
     */
    [[nodiscard]] float new_infections(sird const &c_state) const {
        float aux = 0;
        for(auto neighbor: neighbors) {
            sird n = state.neighbors_state.at(neighbor);
            mc v = state.neighbors_vicinity.at(neighbor);
            aux += n.infected * (float) n.population * v.mobility * v.connectivity;
        }
        return std::min(c_state.susceptible, c_state.susceptible * cell_config.virulence * aux / (float) c_state.population);
    }

    /**
     * Auxiliary method to compute the percentage of new recoveries. This method MUST be constant. Otherwise, it won't compile
     * @param c_state current state of the cell
     * @return percentage of new recoveries
     */
    [[nodiscard]] float new_recoveries(sird const &c_state) const {
        return c_state.infected * cell_config.recovery;
    }

    /**
     * Auxiliary method to compute the percentage of new susceptible people. This method MUST be constant. Otherwise, it won't compile
     * @param c_state current state of the cell
     * @return percentage of new susceptible people
     */
    [[nodiscard]] float new_susceptibles(sird const &c_state) const {
        return c_state.recovered * (1 - cell_config.immunity);
    }

    /**
     * Auxiliary method to compute the percentage of new deceases. This method MUST be constant. Otherwise, it won't compile
     * @param c_state current state of the cell
     * @return percentage of new deceases
     */
    [[nodiscard]] float new_deceases(sird const &c_state) const {
        return c_state.infected * cell_config.fatality;
    }
};
#endif //CELLDEVS_TUTORIAL_1_4_SPATIAL_SIRDS_SIRDS_CELL_HPP
