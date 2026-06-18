#!/bin/bash

#----------------------------------------------------------------------------
# PHP + uAMQP = PHUAMQP
# Scritpt arguments:
#   PHUAMQP_PHP_MAJOR_VERSION: The major version of PHP to build against (e.g., 8.3)
#   PHUAMQP_PHP_CPP_VERSION: The version of PHP-CPP to use (e.g., 2.4.1)
#   PHUAMQP_LIBS_BUILD_DIR: The directory where Azure C Shared Utility and uAMQP C will be built (defaults to PHUAMQP_EXT_DIR/libs-build)
# variables:
#   PHP_MAJOR_VERSION: The major version of PHP to build against (e.g., 8.3)
#   PHP_CPP_VERSION: The version of PHP-CPP to use (e.g., 2.4.1)
#   UAMQP_EXT_DIR: The directory containing the PHP extension source code (defaults to current script directory)
#   UAMQP_LIBS_BUILD_DIR: The directory where Azure C Shared Utility and uAMQP C will be built (defaults to UAMQP_EXT_DIR/libs-build)
# Azure uAMQP C + PHP Extension Build Script
# This script automates the build and installation of the Azure uAMQP C library
# and the corresponding PHP extension. It is designed to be run in a Debian-based
# environment with PHP 8.3.
#----------------------------------------------------------------------------
export DEBIAN_FRONTEND="noninteractive"
export UAMQP_EXT_DIR
export PHP_MAJOR_VERSION="8.3"
export PHP_CPP_VERSION="2.4.1"

UAMQP_EXT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -n "${PHUAMQP_PHP_MAJOR_VERSION:-}" ]; then
    PHP_MAJOR_VERSION="${PHUAMQP_PHP_MAJOR_VERSION}"
fi
if [ -n "${PHUAMQP_PHP_CPP_VERSION:-}" ]; then
    PHP_CPP_VERSION="${PHUAMQP_PHP_CPP_VERSION}"
fi
export UAMQP_LIBS_BUILD_DIR="${UAMQP_EXT_DIR}/libs-build"

echo "============================================================================="
echo "Starting Azure uAMQP C + PHP Extension Build"
echo "Create UAMQP_LIBS_BUILD_DIR"
mkdir -p "${UAMQP_LIBS_BUILD_DIR}"
echo "Build directory: ${UAMQP_LIBS_BUILD_DIR} created"

echo "============================================================================="
echo "Azure uAMQP C + PHP Extension Build Script"
echo "============================================================================="
echo "Target PHP Version: ${PHP_MAJOR_VERSION}"
echo "Libraries Build Directory: ${UAMQP_LIBS_BUILD_DIR}"
echo ""


# ============================================================================
# Step 1: Install Dependencies
# ============================================================================
echo ""
echo "=== Step 1: Installing Dependencies ==="
apt-get update -qq

apt-get install -y \
    wget \
    make \
    g++ \
    gcc \
    php${PHP_MAJOR_VERSION}-dev \
    php${PHP_MAJOR_VERSION}-cli \
    git \
    cmake \
    build-essential \
    curl \
    libcurl4-openssl-dev \
    libssl-dev \
    uuid-dev \
    pkg-config

echo "✓ Dependencies installed"
sleep 2
if [ -t 1 ] && command -v clear >/dev/null 2>&1; then
    clear || true
fi

# ============================================================================
# Step 2: Install Azure C Shared Utility
# ============================================================================
echo ""
echo "=== Step 2: Building Azure C Shared Utility ==="
cd "${UAMQP_LIBS_BUILD_DIR}" || exit 1

# Check if repository already exists
if [ -d "azure-c-shared-utility" ]; then
    echo "✓ Repository already exists, using existing directory"
    cd azure-c-shared-utility || exit 1
    # Clean previous build
    rm -rf cmake
else
    echo "Cloning azure-c-shared-utility repository..."
    git clone --recursive --depth 1 https://github.com/Azure/azure-c-shared-utility.git
    cd azure-c-shared-utility || exit 1

    if [ ! -d ".git" ]; then
        echo "✗ Failed to clone azure-c-shared-utility"
        exit 1
    fi
    echo "✓ Repository cloned"
fi

mkdir -p cmake
cd cmake || exit 1

# Disable -Werror to avoid format-truncation warnings being treated as errors
# BUILD_SHARED_LIBS=ON is CRITICAL - without it, only static libraries (.a) are built
cmake -Duse_installed_dependencies=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DCMAKE_C_FLAGS="-Wno-error=format-truncation -Wno-error=stringop-truncation -Wno-error" \
      ..
if [ $? -ne 0 ]; then
    echo "✗ CMake configuration failed for azure-c-shared-utility"
    exit 1
fi
echo "✓ CMake configured"

cmake --build . --target install -j"$(nproc)"
if [ $? -ne 0 ]; then
    echo "✗ Build failed for azure-c-shared-utility"
    exit 1
fi
echo "✓ Azure C Shared Utility installed"

# Verify installation - check for shared libraries
AZIOT_SO=$(find /usr/local/lib /usr/lib -name "libaziotsharedutil.so*" -type f 2>/dev/null | head -1)
AZIOT_A=$(find /usr/local/lib /usr/lib -name "libaziotsharedutil.a" -type f 2>/dev/null | head -1)

if [ -n "${AZIOT_SO}" ]; then
    echo "✓ Found Azure C Shared Utility as shared library: ${AZIOT_SO}"
elif [ -n "${AZIOT_A}" ]; then
    echo "✓ Found Azure C Shared Utility as static library: ${AZIOT_A}"
else
    echo "⚠ Warning: azure-c-shared-utility library not found in expected locations"
fi

# ============================================================================
# Step 3: Install Azure uAMQP C
# ============================================================================
echo ""
echo "=== Step 3: Building Azure uAMQP C ==="
cd "${UAMQP_LIBS_BUILD_DIR}" || exit 1

# Check if repository already exists
if [ -d "azure-uamqp-c" ]; then
    echo "✓ Repository already exists, using existing directory"
    cd azure-uamqp-c || exit 1
    # Clean previous build
    rm -rf cmake
else
    echo "Cloning azure-uamqp-c repository..."
    git clone --recursive --depth 1 https://github.com/Azure/azure-uamqp-c.git
    cd azure-uamqp-c || exit 1

    if [ ! -d ".git" ]; then
        echo "✗ Failed to clone azure-uamqp-c"
        exit 1
    fi
    echo "✓ Repository cloned"
fi

mkdir -p cmake
cd cmake || exit 1

# Disable -Werror to avoid format-truncation and strict-aliasing warnings being treated as errors
# The azure-uamqp-c code has warnings that would fail the build with -Werror
# BUILD_SHARED_LIBS=ON is CRITICAL - without it, only static libraries (.a) are built
cmake -Duse_installed=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DCMAKE_C_FLAGS="-Wno-error=format-truncation -Wno-error=stringop-truncation -Wno-error=strict-aliasing -Wno-error" \
      ..
if [ $? -ne 0 ]; then
    echo "✗ CMake configuration failed for azure-uamqp-c"
    exit 1
fi
echo "✓ CMake configured"

cmake --build . --target install -j"$(nproc)"
if [ $? -ne 0 ]; then
    echo "✗ Build failed for azure-uamqp-c"
    exit 1
fi
echo "✓ Azure uAMQP C installed"

# Ensure /usr/local/lib is in ldconfig path
echo "/usr/local/lib" > /etc/ld.so.conf.d/local.conf

# Update ldconfig
ldconfig

# Verify installation
echo ""
echo "Verifying uAMQP C library installation..."

# Check for shared libraries (.so)
UAMQP_SO=$(find /usr/local/lib /usr/lib -name "libuamqp.so*" -type f 2>/dev/null | head -1)
UAMQP_A=$(find /usr/local/lib /usr/lib -name "libuamqp.a" -type f 2>/dev/null | head -1)

if [ -n "${UAMQP_SO}" ]; then
    echo "✓ Found uAMQP shared library: ${UAMQP_SO}"
    ls -lh "${UAMQP_SO}"
    if ldconfig -p | grep -q "libuamqp.so"; then
        echo "✓ Library is in ldconfig cache"
    fi
elif [ -n "${UAMQP_A}" ]; then
    echo "✗ ERROR: Only static library found: ${UAMQP_A}"
    echo "PHP extensions require shared libraries (.so), not static libraries!"
    exit 1
else
    echo "uAMQP library not found!!!"
fi

sleep 2

# ============================================================================
# Step 4: Install PHP-CPP
# ============================================================================
echo ""
echo "=== Step 4: Building PHP-CPP ==="
cd "${UAMQP_LIBS_BUILD_DIR}" || exit 1

# PHP-CPP v${PHP_CPP_VERSION} supports PHP 8+
wget -q "https://github.com/CopernicaMarketingSoftware/PHP-CPP/archive/v${PHP_CPP_VERSION}.tar.gz"
if [ $? -ne 0 ]; then
    echo "Failed to download PHP-CPP"
    exit 1
fi

tar xzf "v${PHP_CPP_VERSION}.tar.gz"
cd "PHP-CPP-${PHP_CPP_VERSION}" || exit 1

# Check PHP version and set flags
PHP_VERSION=$(php${PHP_MAJOR_VERSION} -r "echo PHP_VERSION;")
echo "Building PHP-CPP for PHP ${PHP_VERSION}"

make -j"$(nproc)"
if [ $? -ne 0 ]; then
    echo "✗ PHP-CPP build failed"
    exit 1
fi
echo "✓ PHP-CPP built"

make install
if [ $? -ne 0 ]; then
    echo "PHP-CPP installation failed!!!"
    exit 1
fi
echo "✓ PHP-CPP installed"

# Verify PHP-CPP installation
if [ -f "/usr/lib/libphpcpp.so" ] || [ -f "/usr/local/lib/libphpcpp.so" ]; then
    echo "✓ PHP-CPP library found"
    ldconfig
else
    echo "PHP-CPP library not found!"
    exit 1
fi


sleep 2

# ============================================================================
# Step 5: Build PHP Extension
# ============================================================================
echo ""
echo "=== Step 5: Building Azure uAMQP PHP Extension ==="

# Verify that required headers are installed
echo "Verifying required headers are installed..."
MISSING_HEADERS=0

if [ ! -d "/usr/local/include/azureiot/azure_uamqp_c" ]; then
    echo "  ✗ Missing: /usr/local/include/azureiot/azure_uamqp_c/"
    MISSING_HEADERS=1
else
    echo "  ✓ Found: azure_uamqp_c headers"
fi

if [ ! -d "/usr/local/include/c_logging/v2" ]; then
    echo "  ✗ Missing: /usr/local/include/c_logging/v2/"
    MISSING_HEADERS=1
else
    echo "  ✓ Found: c_logging headers"
fi

if [ ! -d "/usr/local/include/azure_c_shared_utility" ]; then
    echo "  ✗ Missing: /usr/local/include/azure_c_shared_utility/"
    MISSING_HEADERS=1
else
    echo "  ✓ Found: azure_c_shared_utility headers"
fi

if [ $MISSING_HEADERS -eq 1 ]; then
    echo ""
    echo "✗ ERROR: Required headers are missing!"
    echo "This usually means Step 2 or Step 3 didn't install headers properly."
    echo "Available include directories:"
    ls -la /usr/local/include/ | head -20
    exit 1
fi
echo "✓ All required headers found"
echo ""

cd "${UAMQP_LIBS_BUILD_DIR}" || exit 1
cd "${UAMQP_EXT_DIR}" || exit 1

# Check if Makefile exists
if [ ! -f "Makefile" ]; then
    echo "⚠ No Makefile found in ${UAMQP_EXT_DIR}"
    echo "Directory contents:"
    ls -la
    echo ""
    echo "⚠ Checking for alternative build method"
    if [ -f "CMakeLists.txt" ]; then
        mkdir -p build
        cd build || exit 1
        cmake .. -DCMAKE_BUILD_TYPE=Release
        cmake --build . -j"$(nproc)"
    else
        echo "✗ No build system found (Makefile or CMakeLists.txt)"
        exit 1
    fi
else
    echo "✓ Found Makefile in ${UAMQP_EXT_DIR}"
    echo ""
    echo "Verifying Makefile has correct include paths..."

    # Check if Makefile has the required include paths
    if grep -q "I/usr/local/include/azureiot" Makefile; then
        echo "✓ Makefile already configured with correct include paths"
    else
        echo "⚠ Makefile missing include paths - attempting to fix..."

        # Add proper include paths using INSTALLED header locations
        INCLUDES="-I/usr/local/include"
        INCLUDES="${INCLUDES} -I/usr/local/include/azureiot"           # For azure_uamqp_c/uamqp.h
        INCLUDES="${INCLUDES} -I/usr/local/include/c_logging/v2"       # For c_logging/logger.h
        INCLUDES="${INCLUDES} -I/usr/local/include/macro_utils"        # For macro_utils
        INCLUDES="${INCLUDES} -I/usr/local/include/umock_c"            # For umock_c

        # Update COMPILER_FLAGS variable
        sed -i "s|COMPILER_FLAGS\s*=\s*\(.*\)-fpic -o|COMPILER_FLAGS      = \1-fpic ${INCLUDES} -o|g" Makefile

        # Update linker flags to include library paths
        sed -i 's|LINKER_FLAGS\s*=\s*-shared|LINKER_FLAGS        = -shared -L/usr/local/lib|g' Makefile

        # Verify fix
        if grep -q "I/usr/local/include/azureiot" Makefile; then
            echo "✓ Makefile successfully updated"
        else
            echo "✗ Failed to update Makefile automatically"
            echo "Please ensure Makefile COMPILER_FLAGS includes: ${INCLUDES}"
            exit 1
        fi
    fi

    echo ""
    echo "Current COMPILER_FLAGS:"
    grep "^COMPILER_FLAGS" Makefile | sed 's/^/  /'
    echo ""


    make -j"$(nproc)"
    if [ $? -ne 0 ]; then
        echo "✗ PHP extension build failed"
        echo ""
        echo "Displaying Makefile compiler line for debugging:"
        grep "g++ -Wall -c" Makefile | head -1
        exit 1
    fi
    echo "✓ PHP extension built"
fi

make install
if [ $? -ne 0 ]; then
    echo "⚠ make install failed, trying manual installation"

    # Find the built extension
    EXTENSION_FILE=$(find . -name "*.so" -type f 2>/dev/null | head -1)
    if [ -n "${EXTENSION_FILE}" ]; then
        PHP_EXT_DIR=$(php${PHP_MAJOR_VERSION} -r "echo ini_get('extension_dir');")
        cp "${EXTENSION_FILE}" "${PHP_EXT_DIR}/"
        echo "✓ Extension manually copied to ${PHP_EXT_DIR}"
    else
        echo "✗ Could not find built extension file"
        exit 1
    fi
fi


sleep 2

# ============================================================================
# Step 6: Configure PHP Extension
# ============================================================================
echo ""
echo "=== Step 6: Configuring PHP Extension ==="

PHP_EXT_DIR=$(php${PHP_MAJOR_VERSION} -r "echo ini_get('extension_dir');")
echo "PHP extension directory: ${PHP_EXT_DIR}"

# Find the extension name (could be uamqp.so or azure_uamqp.so)
EXTENSION_NAME=""
if [ -f "${PHP_EXT_DIR}/uamqp.so" ]; then
    EXTENSION_NAME="uamqp.so"
elif [ -f "${PHP_EXT_DIR}/azure_uamqp.so" ]; then
    EXTENSION_NAME="azure_uamqp.so"
else
    echo "⚠ Extension not found in ${PHP_EXT_DIR}, searching..."
    EXTENSION_FILE=$(find "${PHP_EXT_DIR}" -name "*amqp*.so" -type f 2>/dev/null | head -1)
    if [ -n "${EXTENSION_FILE}" ]; then
        EXTENSION_NAME=$(basename "${EXTENSION_FILE}")
        echo "✓ Found extension: ${EXTENSION_NAME}"
    else
        echo "✗ No AMQP extension found"
        echo "Contents of ${PHP_EXT_DIR}:"
        ls -la "${PHP_EXT_DIR}/"
        exit 1
    fi
fi

echo "Extension file: ${EXTENSION_NAME}"

# Create PHP module configuration
MODULE_AVAILABLE_DIR="/etc/php/${PHP_MAJOR_VERSION}/mods-available"
mkdir -p "${MODULE_AVAILABLE_DIR}"

cat > "${MODULE_AVAILABLE_DIR}/uamqp.ini" << EOF
; Azure uAMQP PHP Extension
extension=${EXTENSION_NAME}
EOF

echo "✓ Extension configuration created"

# Enable the module
if command -v phpenmod &> /dev/null; then
    phpenmod -v "${PHP_MAJOR_VERSION}" uamqp
    echo "✓ Extension enabled via phpenmod"
else
    echo "phpenmod not found, enabling manually..."
    for CONF_DIR in /etc/php/${PHP_MAJOR_VERSION}/{cli,fpm,cgi,apache2}; do
        if [ -d "${CONF_DIR}/conf.d" ]; then
            ln -sf "${MODULE_AVAILABLE_DIR}/uamqp.ini" "${CONF_DIR}/conf.d/20-uamqp.ini"
            echo "  ✓ Enabled for $(basename ${CONF_DIR})"
        fi
    done
fi

# Configure PHP-FPM environment if needed
FPM_POOL_CONF="/etc/php/${PHP_MAJOR_VERSION}/fpm/pool.d/www.conf"
if [ -f "${FPM_POOL_CONF}" ]; then
    if ! grep -q "LD_LIBRARY_PATH.*uamqp" "${FPM_POOL_CONF}" 2>/dev/null; then
        echo "; uAMQP library path" >> "${FPM_POOL_CONF}"
        echo "env[LD_LIBRARY_PATH] = /usr/local/lib:/usr/lib:/usr/lib/x86_64-linux-gnu" >> "${FPM_POOL_CONF}"
        echo "✓ PHP-FPM environment configured"
    fi
fi

# ============================================================================
# Step 7: Final Verification
# ============================================================================
echo ""
echo "=== Step 7: Final Verification ==="

# Set LD_LIBRARY_PATH for verification
export LD_LIBRARY_PATH="/usr/local/lib:/usr/lib:/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}"

# Check extension file
echo ""
echo "1. Extension file check:"
if [ -f "${PHP_EXT_DIR}/${EXTENSION_NAME}" ]; then
    echo "   ✓ Extension file exists: ${PHP_EXT_DIR}/${EXTENSION_NAME}"
    echo "   File size: $(ls -lh ${PHP_EXT_DIR}/${EXTENSION_NAME} | awk '{print $5}')"
    echo "   File type:"
    file "${PHP_EXT_DIR}/${EXTENSION_NAME}" | sed 's/^/     /'
else
    echo "   ✗ Extension file not found"
    exit 1
fi

# Check library dependencies
echo ""
echo "2. Library dependencies check:"
ldd "${PHP_EXT_DIR}/${EXTENSION_NAME}" 2>&1 | sed 's/^/     /'
if ldd "${PHP_EXT_DIR}/${EXTENSION_NAME}" 2>&1 | grep -q "not found"; then
    echo "   ⚠ Warning: Missing library dependencies detected"
fi

# Check if PHP loads the extension
echo ""
echo "3. PHP extension loading check:"
if php${PHP_MAJOR_VERSION} -m 2>&1 | grep -qi "uamqp"; then
    echo "   ✓✓✓ SUCCESS: uAMQP extension is loaded in PHP!"
    echo ""
    echo "   Extension details:"
    php${PHP_MAJOR_VERSION} -r "if (extension_loaded('uamqp')) { echo 'Extension: uamqp' . PHP_EOL; echo 'Status: LOADED' . PHP_EOL; } else { echo 'Not loaded' . PHP_EOL; }"
else
    echo "   ✗ ERROR: uAMQP extension is NOT loaded"
    echo ""
    echo "   Troubleshooting:"
    echo "   - PHP Version: $(php${PHP_MAJOR_VERSION} --version | head -1)"
    echo "   - Extension dir: $(php${PHP_MAJOR_VERSION} -r 'echo ini_get("extension_dir");')"
    echo "   - Loaded modules:"
    php${PHP_MAJOR_VERSION} -m | head -20 | sed 's/^/       /'
    echo "   - PHP errors:"
    php${PHP_MAJOR_VERSION} -d display_errors=1 -r "echo 'test';" 2>&1 | grep -i "error\|warning" | sed 's/^/       /' || echo "       (no errors)"
    exit 1
fi

# ============================================================================
# Cleanup
# ============================================================================
echo ""
echo "=== Cleanup ==="
echo "Removing temporary build files..."
rm -rf "${UAMQP_LIBS_BUILD_DIR}"
echo "✓ Cleanup completed"

echo ""
echo "============================================================================="
echo "✓✓✓ Azure uAMQP PHP Extension Installation SUCCESS ✓✓✓"
echo "============================================================================="
echo "Extension: ${EXTENSION_NAME}"
echo "Location: ${PHP_EXT_DIR}/"
echo "Status: LOADED in PHP ${PHP_MAJOR_VERSION}"
echo "============================================================================="
