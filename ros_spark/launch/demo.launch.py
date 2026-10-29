import launch
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    container = ComposableNodeContainer(
        name="SparkContainer",
        namespace="",
        package="rclcpp_components",
        executable="component_container",
        composable_node_descriptions=[
            ComposableNode(
                package="ros_spark",
                plugin="ros_spark::SparkMax",
                name="demo",
                parameters=[{"id": 1}],
            ),
        ],
        output="screen",
    )

    return launch.LaunchDescription([container])
