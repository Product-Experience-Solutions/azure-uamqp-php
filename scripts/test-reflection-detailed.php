<?php

ini_set('error_reporting', E_ALL);
ini_set('display_errors', '1');

echo "Testing PHP uAMQP Extension - Detailed Reflection\n";
echo "=================================================\n\n";

$uAmqpExtension = "uamqpphpbinding";

// Check if extension is loaded
if (!extension_loaded($uAmqpExtension)) {
    die("ERROR: $uAmqpExtension is not loaded!\n");
}

echo "✓ Extension loaded successfully\n\n";

$extInfo = new ReflectionExtension($uAmqpExtension);
echo "Extension: " . $extInfo->getName() . "\n";
echo "Version: " . $extInfo->getVersion() . "\n\n";

echo "Classes:\n";
$classes = $extInfo->getClasses();
foreach ($classes as $className => $class) {
    echo "  - $className\n";
}
echo "\n";

echo "Processing each class...\n\n";
foreach ($classes as $className => $class) {
    echo "=== $className ===\n";

    try {
        $r = new ReflectionClass($className);
        echo "Class reflection created successfully\n";

        echo "Getting methods...\n";
        $methods = $r->getMethods();
        echo "Found " . count($methods) . " methods\n";

        if (count($methods) > 0) {
            echo "Methods:\n";
            foreach ($methods as $method) {
                try {
                    echo "  - " . $method->getName();

                    // Try to get parameters safely
                    try {
                        $params = $method->getParameters();
                        echo " (";
                        $paramNames = [];
                        foreach ($params as $param) {
                            $paramNames[] = $param->getName();
                        }
                        echo implode(", ", $paramNames);
                        echo ")";
                    } catch (Exception $e) {
                        echo " - Error getting parameters: " . $e->getMessage();
                    }

                    echo "\n";
                } catch (Exception $e) {
                    echo "  - ERROR processing method: " . $e->getMessage() . "\n";
                }
            }
        } else {
            echo "No methods found\n";
        }

    } catch (Exception $e) {
        echo "ERROR: " . $e->getMessage() . "\n";
    }

    echo "\n";
}

echo "Done!\n";

