// Read anchor, secondary, native position, and direction triples per row; emit actual core results.
#include "camera_geometry.hpp"
#include <iomanip>
#include <iostream>
int main() {
    efp::Vec3 anchor{}, secondary{}, native{}, direction{};
    std::size_t count = 0;
    std::cout << std::fixed << std::setprecision(6);
    while (std::cin >> anchor[0]) {
        std::cin >> anchor[1] >> anchor[2];
        for (auto* vector : {&secondary,&native,&direction}) for (auto& value : *vector) std::cin >> value;
        if (!std::cin) return 1;
        const auto target = efp::anchored_position(anchor,secondary,direction,efp::Settings{});
        if (!efp::plausible_anchor(anchor,secondary,native) || !target) std::cout << "rejected\n";
        else std::cout << (*target)[0] << ' ' << (*target)[1] << ' ' << (*target)[2] << '\n';
        ++count;
    }
    return count && std::cin.eof() ? 0 : 1;
}
