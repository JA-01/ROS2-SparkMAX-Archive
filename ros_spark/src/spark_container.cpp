#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/component_manager.hpp"

#include <memory>
#include <string>

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    auto exec = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    auto node = std::make_shared<rclcpp_components::ComponentManager>(exec);

    exec->add_node(node);
    exec->spin();

    return 0;
}
