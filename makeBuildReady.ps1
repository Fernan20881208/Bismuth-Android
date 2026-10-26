Rename-Item -Path "build\iliashdz.bismuth.geode" -NewName "iliashdz.bismuth.zip"
Compress-Archive -Path "build\iliashdz.bismuth.pdb" -Update -DestinationPath "build\iliashdz.bismuth.zip"
Rename-Item -Path "build\iliashdz.bismuth.zip" -NewName "iliashdz.bismuth.geode"