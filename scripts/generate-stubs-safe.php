<?php

ini_set('error_reporting', E_ALL);
ini_set('display_errors', '1');

echo "PHP Class Stubs Generator for uAMQP Extension (Safe Version)\n";
echo "=============================================================\n\n";

$uAmqpExtension = "uamqpphpbinding";

// Check if extension is loaded
if (!extension_loaded($uAmqpExtension)) {
    die("ERROR: $uAmqpExtension is not loaded!\n");
}

echo "✓ Extension loaded successfully\n\n";

$extInfo = new ReflectionExtension($uAmqpExtension);
echo "Extension: " . $extInfo->getName() . "\n";
echo "Version: " . $extInfo->getVersion() . "\n\n";

// Generate stub file content
$stubContent = "<?php\n\n";
$stubContent .= "/**\n";
$stubContent .= " * PHP Stubs for {$extInfo->getName()} extension\n";
$stubContent .= " * Version: {$extInfo->getVersion()}\n";
$stubContent .= " * Auto-generated on " . date('Y-m-d H:i:s') . "\n";
$stubContent .= " *\n";
$stubContent .= " * Note: This is a safe version that avoids deep reflection\n";
$stubContent .= " * to prevent segmentation faults with PHP-CPP extensions.\n";
$stubContent .= " */\n\n";

// Process classes
$classes = $extInfo->getClasses();
echo "Found " . count($classes) . " classes\n\n";

foreach ($classes as $className => $class) {
    // Skip PhpCpp internal classes
    if (strpos($className, 'PhpCpp::') === 0) {
        echo "Skipping: $className\n";
        continue;
    }

    echo "Processing: $className\n";

    $namespace = $class->getNamespaceName();
    $shortName = $class->getShortName();

    $stubContent .= "namespace $namespace {\n\n";
    $stubContent .= "    /**\n";
    $stubContent .= "     * Class $shortName\n";
    $stubContent .= "     */\n";
    $stubContent .= "    class $shortName {\n\n";

    // Get methods safely
    try {
        $methods = $class->getMethods();
        echo "  Found " . count($methods) . " methods\n";

        foreach ($methods as $method) {
            // Only include methods declared in this class
            if ($method->getDeclaringClass()->getName() !== $className) {
                continue;
            }

            $methodName = $method->getName();
            echo "    - $methodName\n";

            // Get parameter names safely (avoid type reflection entirely)
            $paramNames = [];
            try {
                foreach ($method->getParameters() as $param) {
                    $paramNames[] = $param->getName();
                }
            } catch (Throwable $e) {
                // If we can't get parameters, use generic names
                $paramNames = ["..."];
            }

            // Generate method stub
            $stubContent .= "        /**\n";
            $stubContent .= "         * Method: $methodName\n";
            foreach ($paramNames as $paramName) {
                $stubContent .= "         * @param mixed \$$paramName\n";
            }
            $stubContent .= "         * @return mixed\n";
            $stubContent .= "         */\n";
            $stubContent .= "        public function $methodName(";

            // Add parameter list without types
            $paramList = [];
            foreach ($paramNames as $paramName) {
                if ($paramName !== "...") {
                    $paramList[] = "\$$paramName";
                } else {
                    $paramList[] = "...";
                }
            }
            $stubContent .= implode(', ', $paramList);

            $stubContent .= ") {}\n\n";
        }
    } catch (Throwable $e) {
        echo "  ERROR: " . $e->getMessage() . "\n";
    }

    $stubContent .= "    }\n";
    $stubContent .= "}\n\n";
}

// Output to console
echo "\n" . str_repeat("=", 60) . "\n";
echo "Generated Stubs:\n";
echo str_repeat("=", 60) . "\n\n";
echo $stubContent;

// Also save to file
$stubFile = __DIR__ . '/uamqpphpbinding-stubs.php';
file_put_contents($stubFile, $stubContent);
echo "\n" . str_repeat("=", 60) . "\n";
echo "✓ Stubs saved to: $stubFile\n";
echo str_repeat("=", 60) . "\n";

