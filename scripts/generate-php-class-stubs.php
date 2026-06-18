<?php

ini_set('error_reporting', E_ALL);
ini_set('display_errors', '1');

// ============ Helper Functions ============

function generateFunctionStub(ReflectionFunction $func): string {
    $stub = "/**\n";
    $stub .= " * " . ($func->getDocComment() ?: "Function: {$func->getName()}") . "\n";
    $stub .= " */\n";
    $stub .= "function {$func->getName()}(";
    
    $params = [];
    foreach ($func->getParameters() as $param) {
        $params[] = formatParameter($param);
    }
    $stub .= implode(', ', $params);
    $stub .= ")";
    
    if ($func->hasReturnType()) {
        $stub .= ": " . $func->getReturnType();
    }
    
    $stub .= " {}\n\n";
    
    return $stub;
}

function generateClassStub(ReflectionClass $class): string {
    echo "    Getting namespace...\n";
    $namespace = $class->getNamespaceName();
    echo "    Namespace: $namespace\n";

    $stub = "namespace " . $namespace . " {\n\n";

    $stub .= "    /**\n";
    $stub .= "     * Class {$class->getShortName()}\n";
    $stub .= "     */\n";
    $stub .= "    class {$class->getShortName()}";
    
    echo "    Checking parent class...\n";
    // Parent class
    try {
        $parent = $class->getParentClass();
        if ($parent) {
            $stub .= " extends \\" . $parent->getName();
        }
    } catch (Throwable $e) {
        echo "    Error getting parent: " . $e->getMessage() . "\n";
    }
    
    $stub .= " {\n\n";
    
    echo "    Getting methods...\n";
    // Methods
    try {
        $methods = $class->getMethods();
        echo "    Found " . count($methods) . " methods\n";
        foreach ($methods as $method) {
            // Skip inherited methods if you want
            if ($method->getDeclaringClass()->getName() !== $class->getName()) {
                continue;
            }

            echo "      Processing method: " . $method->getName() . "\n";
            try {
                $stub .= generateMethodStub($method);
                echo "      Done with method: " . $method->getName() . "\n";
            } catch (Throwable $e) {
                echo "      ERROR with method: " . $e->getMessage() . "\n";
                // Add a placeholder stub for the failed method
                $stub .= "        // Method: {$method->getName()} - reflection failed\n";
                $stub .= "        public function {$method->getName()}() {}\n\n";
            }
        }
    } catch (Throwable $e) {
        echo "    Error getting methods: " . $e->getMessage() . "\n";
    }
    
    $stub .= "    }\n";
    $stub .= "}\n\n";
    
    return $stub;
}

function generateMethodStub(ReflectionMethod $method): string {
    $stub = "        /**\n";
    $stub .= "         * Method: {$method->getName()}\n";
    
    //echo "        Getting parameter list for docs...\n";
    // Document parameters - safely
    try {
        $params = $method->getParameters();
        //echo "        Found " . count($params) . " parameters\n";
        foreach ($params as $idx => $param) {
            //echo "          Documenting param $idx: " . $param->getName() . "\n";
            try {
                $paramType = getParameterTypeString($param);
                $stub .= "         * @param " . $paramType . " \${$param->getName()}";
                try {
                    if ($param->isOptional() && $param->isDefaultValueAvailable()) {
                        $stub .= " (optional, default: " . var_export($param->getDefaultValue(), true) . ")";
                    }
                } catch (Throwable $e) {
                    // Ignore errors getting default value
                }
                $stub .= "\n";
            } catch (Throwable $e) {
                //echo "          Error with param: " . $e->getMessage() . "\n";
                $stub .= "         * @param mixed \$param$idx\n";
            }
        }
    } catch (Throwable $e) {
        //echo "        Error getting parameters: " . $e->getMessage() . "\n";
        $stub .= "         * @param mixed ...\n";
    }
    
    // Document return type - safely
    try {
        if ($method->hasReturnType()) {
            $returnType = $method->getReturnType();
            $stub .= "         * @return " . (string)$returnType . "\n";
        }
    } catch (Throwable $e) {
        // Ignore errors getting return type
    }
    
    $stub .= "         */\n";
    $stub .= "        public ";
    
    try {
        if ($method->isStatic()) {
            $stub .= "static ";
        }
    } catch (Throwable $e) {
        // Ignore
    }
    
    $stub .= "function {$method->getName()}(";
    
    //echo "        Formatting parameters...\n";
    $paramStrings = [];
    try {
        $params = $method->getParameters();
        foreach ($params as $idx => $param) {
            //echo "          Formatting param $idx: " . $param->getName() . "\n";
            try {
                $paramStrings[] = formatParameter($param);
            } catch (Throwable $e) {
                //echo "          Error formatting param: " . $e->getMessage() . "\n";
                $paramStrings[] = "\$param$idx";
            }
        }
    } catch (Throwable $e) {
        //echo "        Error getting parameters for formatting: " . $e->getMessage() . "\n";
        // If we can't get parameters safely, just use empty
    }
    $stub .= implode(', ', $paramStrings);
    $stub .= ")";
    
    // Return type - safely
    try {
        if ($method->hasReturnType()) {
            $returnType = $method->getReturnType();
            if ($returnType instanceof ReflectionNamedType) {
                $nullable = "";
                try {
                    if ($returnType->allowsNull()) {
                        $nullable = "?";
                    }
                } catch (Throwable $e) {
                    // Ignore
                }
                $stub .= ": " . $nullable . (string)$returnType;
            }
        }
    } catch (Throwable $e) {
        // Ignore errors getting return type
    }
    
    $stub .= " {}\n\n";
    
    return $stub;
}

function formatParameter(ReflectionParameter $param): string {
    $result = "";
    
    // Type hint - be very careful with PHP-CPP extensions
    try {
        if ($param->hasType()) {
            $type = $param->getType();
            if ($type instanceof ReflectionNamedType) {
                $typeName = (string)$type; // Convert to string safely
                // Check for nullable
                try {
                    if ($type->allowsNull() && !$param->isDefaultValueAvailable()) {
                        $result .= "?";
                    }
                } catch (Throwable $e) {
                    // Ignore errors on allowsNull check
                }
                $result .= $typeName . " ";
            }
        }
    } catch (Throwable $e) {
        // If type reflection fails, just skip the type hint
    }
    
    // Parameter name
    $result .= "\${$param->getName()}";
    
    // Default value
    try {
        if ($param->isOptional() && $param->isDefaultValueAvailable()) {
            $defaultValue = $param->getDefaultValue();
            if (is_bool($defaultValue)) {
                $result .= " = " . ($defaultValue ? "true" : "false");
            } elseif (is_null($defaultValue)) {
                $result .= " = null";
            } elseif (is_string($defaultValue)) {
                $result .= " = " . var_export($defaultValue, true);
            } elseif (is_numeric($defaultValue)) {
                $result .= " = " . $defaultValue;
            } else {
                $result .= " = null /* " . gettype($defaultValue) . " */";
            }
        }
    } catch (Throwable $e) {
        // Ignore errors getting default value
    }
    
    return $result;
}

function getParameterTypeString(ReflectionParameter $param): string {
    try {
        if ($param->hasType()) {
            $type = $param->getType();
            if ($type instanceof ReflectionNamedType) {
                return (string)$type;
            }
        }
    } catch (Throwable $e) {
        // Ignore errors
    }
    return "mixed";
}

// ============ Main Execution ============

echo "PHP Class Stubs Generator for uAMQP Extension\n";
echo "==============================================\n\n";

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
$stubContent .= " */\n\n";

// Process functions
$functions = $extInfo->getFunctions();
if (count($functions) > 0) {
    $stubContent .= "// ============ Functions ============\n\n";
    foreach ($functions as $func) {
        $stubContent .= generateFunctionStub($func);
    }
}

// Process classes
$classes = $extInfo->getClasses();
echo "Found " . count($classes) . " classes\n";
if (count($classes) > 0) {
    $stubContent .= "// ============ Classes ============\n\n";

    foreach ($classes as $className => $class) {
        echo "Processing class: $className\n";
        // Skip PhpCpp internal classes
        if (strpos($className, 'PhpCpp::') === 0) {
            echo "  Skipping PhpCpp internal class\n";
            continue;
        }

        echo "  Generating stub...\n";
        $stubContent .= generateClassStub($class);
        echo "  Done\n";
    }
}

// Output to console
echo $stubContent;

// Also save to file
$stubFile = __DIR__ . '/uamqpphpbinding-stubs.php';
file_put_contents($stubFile, $stubContent);
echo "\n\n✓ Stubs saved to: $stubFile\n";
