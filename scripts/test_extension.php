<?php

echo "Testing PHP uAMQP Extension\n";
echo "=====================\n\n";

$uAmqpExtension = "uamqpphpbinding";

// Check if extension is loaded
if (!extension_loaded($uAmqpExtension)) {
    die("ERROR: $uAmqpExtension is not loaded!\n");
}

echo "✓ Extension loaded successfully\n\n";

$extInfo = new ReflectionExtension($uAmqpExtension);
echo $extInfo->getName() . "\n";
echo "Version: " . $extInfo->getVersion() . "\n";
echo "\n";
echo "Functions\n";
foreach ($extInfo->getFunctions() as $funk) {
    echo $funk->getName(), PHP_EOL;
}

echo "\n";
echo "Classes:\n";
$classes = [];
foreach ($extInfo->getClasses() as $class) {
    echo $class->getName(), PHP_EOL;
    $classes[] = $class;
}

echo "\n";
echo "Methods:\n";
foreach ($classes as $class) {
    $r = new ReflectionClass($class);

    echo "\n=== $class ===\n";

    foreach ($r->getMethods() as $method) {
        echo $method. PHP_EOL;
    }
}
