import { AppSidebar } from "@/components/app-sidebar"
import {
  Breadcrumb,
  BreadcrumbItem,
  BreadcrumbList,
  BreadcrumbPage,
} from "@/components/ui/breadcrumb"
import { Button } from "@/components/ui/button"
import { Card, CardContent, CardFooter, CardHeader, CardTitle } from "@/components/ui/card"
import { Separator } from "@/components/ui/separator"
import {
  SidebarInset,
  SidebarProvider,
  SidebarTrigger,
} from "@/components/ui/sidebar"
import { SiGithub } from '@icons-pack/react-simple-icons';
import { Scale } from "lucide-react"

export default function Page() {
  return (
    <SidebarProvider>
      <AppSidebar />
      <SidebarInset>
        <header className="flex h-16 shrink-0 items-center gap-2 transition-[width,height] ease-linear group-has-[[data-collapsible=icon]]/sidebar-wrapper:h-12">
          <div className="flex items-center gap-2 px-4">
            <SidebarTrigger className="-ml-1" />
            <Separator orientation="vertical" className="mr-2 h-4" />
            <Breadcrumb>
              <BreadcrumbList>
                <BreadcrumbItem>
                  <BreadcrumbPage>About Noraneko</BreadcrumbPage>
                </BreadcrumbItem>
              </BreadcrumbList>
            </Breadcrumb>
          </div>
        </header>
        <div className="flex p-4">
          <Card>
            <CardHeader>
              <CardTitle>About Noraneko Browser</CardTitle>
            </CardHeader>
            <CardContent>
              <p>Noraneko is a browser as testhead of Floorp 12.</p>
              <p>Made by Noraneko Community with ❤</p>
            </CardContent>
            <CardFooter>
              <div>
                  <div>Binaries: Mozilla Public License 2.0 (MPL)</div>
                  <div>
                    <Button asChild>
                      <a href="about:license"><Scale/>License Notice: Know your rights</a>
                    </Button>
                  </div>
                  <div>
                    <Button asChild>
                      <a href="https://github.com/nyanrus/noraneko"><SiGithub/>GitHub Repository: nyanrus/noraneko</a>
                    </Button>
                  </div>
              </div>
            </CardFooter>
          </Card>
        </div>
        {/* <div className="flex flex-1 flex-col gap-4 p-4 pt-0">
          <div className="grid auto-rows-min gap-4 md:grid-cols-3">
            <div className="aspect-video rounded-xl bg-muted/50" />
            <div className="aspect-video rounded-xl bg-muted/50" />
            <div className="aspect-video rounded-xl bg-muted/50" />
          </div>
          <div className="min-h-[100vh] flex-1 rounded-xl bg-muted/50 md:min-h-min" />
        </div> */}
      </SidebarInset>
    </SidebarProvider>
  )
}
