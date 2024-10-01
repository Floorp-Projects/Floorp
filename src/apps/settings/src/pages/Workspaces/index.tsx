import {
  Flex,
  VStack,
  Text,
  Avatar,
  Grid,
  GridItem,
  Progress,
  Link,
  Icon,
} from "@chakra-ui/react";
import Card from "../../components/Card";

export default function Workspaces() {
  return (
    <GridItem display="flex" justifyContent="center">
      <Text fontSize={"2xl"}>Workspaces</Text>
      <VStack align="stretch" alignItems="center" maxW={"900px"} spacing={6}>
        <Text>
          ワークスペースを使用すると、タブやウィンドウを整理し、作業を効率的に管理できます。
        </Text>

        <Grid templateColumns={{ base: "repeat(1, 1fr)", lg: "repeat(2, 1fr)" }} gap={4}>
          <Card
            title="ワークスペースの作成"
            icon={<IconMdiPlus style={{ fontSize: '24px', color: '#3182F6' }} />}
          >
            <Text fontSize="sm">
              新しいワークスペースを作成して、プロジェクトや作業を整理します。
            </Text>
            <Link color="blue.500" fontSize="sm" mt={2}>
              ワークスペースを作成
            </Link>
          </Card>
          <Card
            title="ワークスペースの管理"
            icon={<IconMdiFolder style={{ fontSize: '24px', color: '#ff7708' }} />}
          >
            <Text fontSize="sm">
              既存のワークスペースを表示、編集、削除します。
            </Text>
            <Link color="blue.500" fontSize="sm" mt={2}>
              ワークスペースを管理
            </Link>
          </Card>
          <Card
            title="ワークスペースの設定"
            icon={<IconMdiCog style={{ fontSize: '24px', color: '#137333' }} />}
          >
            <Text fontSize="sm">
              ワークスペースの動作や表示方法をカスタマイズします。
            </Text>
            <Link color="blue.500" fontSize="sm" mt={2}>
              設定を変更
            </Link>
          </Card>
          <Card
            title="ワークスペースの同期"
            icon={<IconMdiSync style={{ fontSize: '24px', color: '#8400ff' }} />}
          >
            <Text fontSize="sm">
              ワークスペースをデバイス間で同期し、どこでも作業を継続できます。
            </Text>
            <Link color="blue.500" fontSize="sm" mt={2}>
              同期設定を管理
            </Link>
          </Card>
        </Grid>
      </VStack>
    </GridItem>
  );
}
