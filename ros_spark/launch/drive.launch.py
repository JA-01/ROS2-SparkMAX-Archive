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
                name="back_right",
                parameters=[{"id": 2, "invert": True}],
            ),
            ComposableNode(
                package="ros_spark",
                plugin="ros_spark::SparkMax",
                name="back_left",
                parameters=[{"id": 5}],
            ),
            #ComposableNode(
            #    package="ros_spark",
            #    plugin="ros_spark::SparkMax",
            #    name="front_left",
            #    parameters=[{"id": 3}],
            #),
            #ComposableNode(
            #    package="ros_spark",
            #    plugin="ros_spark::SparkMax",
            #    name="front_right",
            #    parameters=[{"id": 4, "invert": True}],
            #),
        ],
        output="screen",
    )

    return launch.LaunchDescription([container])
